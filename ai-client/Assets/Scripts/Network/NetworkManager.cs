using System;
using System.Net;
using UnityEngine;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Cysharp.Threading.Tasks;
using NetworkData;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using UnityEditor;
using Utility;

public class NetworkManager : Singleton<NetworkManager>
{
    [SerializeField] private string _serverIp = "127.0.0.1";
    [SerializeField] private ushort _serverPort = 53200;
    [SerializeField] private NetworkInputAction _inputAction;
    public Guid ConnectedUuid { get; private set; }
    
    public Action disconnectAction;
    
    private TcpClient _tcpClient;
    private UdpClient _udpClient;
    private IPEndPoint _serverUdpEndPoint;
    private IPEndPoint _clientUdpEndPoint;
    private NetworkStream _tcpStream;
    private int _maxRetries = 5;
    private readonly object _sentPacketLock = new();
    private readonly object _receivedPacketLock = new();
    private uint _sentPacketCount = 0;
    private uint _receivedPacketCount = 0;

    private uint _netSize;

    private CancellationTokenSource _cancellationTokenSource = new();
    public CancellationToken CToken => _cancellationTokenSource.Token;

    public bool IsOnline { get; private set; }
    public bool IsSendPacketOn { get; private set; }
    private bool IsTryingToConnect { get; set; }

    private void OnEnable()
    {
        _inputAction.connectAction += TryConnectToServer;
        _inputAction.disconnectAction += DisconnectFromServer;
        _inputAction.sendPacketToggleAction += ToggleSendPacket;
    }

    private void OnDisable()
    {
        _inputAction.connectAction -= TryConnectToServer;
        _inputAction.disconnectAction -= DisconnectFromServer;
        _inputAction.sendPacketToggleAction -= ToggleSendPacket;
    }

    private void Start()
    {
        Debug.Log($"NetworkManager Ready");
        IsSendPacketOn = false;
        IsOnline = false;
        IsTryingToConnect = false;
    }

    private void ToggleSendPacket()
    {
        IsSendPacketOn = !IsSendPacketOn;
        Debug.Log($"NetworkManager ToggleSendPacket - {IsSendPacketOn}");
    }

    private void OnDestroy()
    {
        _cancellationTokenSource.Cancel();
        _cancellationTokenSource.Dispose();
        DisconnectFromServer();
    }

    private void TryConnectToServer()
    {
        if (IsOnline)
            return;
        
        if (IsTryingToConnect)
            return;
        
        Debug.Log($"NetworkManager TryConnectToServer");
        ConnectToServer().Forget();
    }

    private async UniTask ConnectToServer()
    {
        if (IsOnline)
            return;

        IsTryingToConnect = true;
        
        try
        {
            _tcpClient = new TcpClient();
            
            // RTT packet pre-allocation
            
            
            await _tcpClient.ConnectAsync(_serverIp, _serverPort);
            _tcpStream = _tcpClient.GetStream();
            
            // Get Udp Port
            await AsyncExchangeUdpPort();
            
            // Receive UUID
            await AsyncReceiveUuid();
            
            lock (_sentPacketLock)
            {
                _sentPacketCount = 0;
            }
        
            lock (_receivedPacketLock)
            {
                _receivedPacketCount = 0;
            }
            
            IsOnline = true;
        }
        catch (Exception ex)
        {
            if (_maxRetries-- > 0)
            {
                Debug.Log(_maxRetries > 0 
                    ? $"Retrying connection to server... {_maxRetries} tries left" 
                    : $"Last retrying connection to server...");
                
                ConnectToServer().Forget();
                return;
            }
            
            Debug.LogError($"failed to connect - {ex.Message}");
#if UNITY_EDITOR
            EditorApplication.isPlaying = false;
#endif
        }

        await UniTask.Yield();

        TcpAsyncReadData().Forget();
        UdpAsyncReadData().Forget();
        
        IsTryingToConnect = false;
    }

    private async UniTask AsyncExchangeUdpPort()
    {
        try
        {
            var udpPortPacketSize = new byte[4];

            // Read the size of the incoming data
            int bytesRead = await _tcpStream.ReadAsync(udpPortPacketSize, 0, udpPortPacketSize.Length, CToken);
            if (bytesRead == 0) // EOF connection closed
                DisconnectFromServer();

            // Convert the size from bytes to uint
            if (BitConverter.IsLittleEndian)
                Array.Reverse(udpPortPacketSize);

            var dataSize = BitConverter.ToUInt32(udpPortPacketSize, 0);
            var dataBuffer = new byte[dataSize];

            // Read the rest of the data
            bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, CToken);
            if (bytesRead == 0) // EOF connection closed
                DisconnectFromServer();

            // Deserialize the data
            RpcPacket udpPortData = ProtoSerializer.DeserializeNetworkData(dataBuffer);
            ushort udpPort = Util.ConvertByteStringToUShort(udpPortData.Data);

            _udpClient = new UdpClient(new IPEndPoint(IPAddress.Any, 0));
            _clientUdpEndPoint = (IPEndPoint)_udpClient.Client.LocalEndPoint!;
            _serverUdpEndPoint = new IPEndPoint(IPAddress.Parse(_serverIp), udpPort);
            Debug.Log($"Udp Client Created - {_serverUdpEndPoint.Address}:{_serverUdpEndPoint.Port}");
            
            // Send the UDP port to the server
            var clientPort = (ushort)_clientUdpEndPoint.Port;
            var sendUdpPortPacket = new RpcPacket
            {
                Method = RpcMethod.UdpPort,
                Data = Util.ConvertUShortToByteString(clientPort)
            };
            
            Debug.Log($"Send Udp Port little endian - {clientPort}");
                
            byte[] sendUdpPortPacketData = sendUdpPortPacket.ToByteArray();
            byte[] udpPortSize = BitConverter.GetBytes(sendUdpPortPacketData.Length);
            if (BitConverter.IsLittleEndian)
               Array.Reverse(udpPortSize);
            
            // Send the UDP port packet
            await _tcpStream.WriteAsync(udpPortSize, 0, udpPortSize.Length, CToken);
            await _tcpStream.WriteAsync(sendUdpPortPacketData, 0, sendUdpPortPacketData.Length, CToken);
            Debug.Log($"Exchanged Udp Port completed");
        }
        catch (Exception ex)
        {
            Debug.LogError($"error in AsyncReceiveUdpPort: {ex.Message}");
        }
        
        await UniTask.Yield();
    }

    private async UniTask AsyncReceiveUuid()
    {
        try
        {
            var sizeBuffer = new byte[sizeof(uint)];

            // Read the size of the incoming data
            int bytesRead = await _tcpStream.ReadAsync(sizeBuffer, 0, sizeBuffer.Length, CToken);
            if (bytesRead == 0) // EOF connection closed
                DisconnectFromServer();

            // Convert the size from bytes to uint
            if (BitConverter.IsLittleEndian)
                Array.Reverse(sizeBuffer);

            _netSize = BitConverter.ToUInt32(sizeBuffer, 0);
            var dataBuffer = new byte[_netSize];
            
            bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, CToken);
            if (bytesRead == 0) // EOF connection closed
                DisconnectFromServer();

            await AsyncProcessRpc(ProtoSerializer.DeserializeNetworkData(dataBuffer));
            Debug.Log($"Uuid received: {ConnectedUuid.ToString()}");
        }
        catch (Exception ex)
        {
            Debug.LogError($"Exception in AsyncReceiveUuid: {ex.Message}");
            DisconnectFromServer();
        }

        _netSize = 0;
        await UniTask.Yield();
    }

    private void DisconnectFromServer()
    {
        if (!IsOnline)
            return;

        _tcpStream?.Close();
        _tcpClient?.Close();
        IsOnline = false;
        IsSendPacketOn = false;

        disconnectAction?.Invoke();

        Debug.Log("disconnected.");
    }
    
    public async UniTask AsyncWriteRpcPacket(RpcPacket data)
    {
        if (!IsOnline)
            return;

        try
        {
            data.Uuid = ProtoSerializer.SerializeUuid(ConnectedUuid);

            byte[] payload = ProtoSerializer.SerializeNetworkData(data);
            var payloadSize = (short)payload.Length;
            byte[] sizeBuffer = BitConverter.GetBytes(payloadSize);
            
            if(BitConverter.IsLittleEndian)
                Array.Reverse(sizeBuffer);

            var packet = new byte[sizeBuffer.Length + payload.Length];
            Buffer.BlockCopy(sizeBuffer, 0, packet, 0, sizeBuffer.Length);
            Buffer.BlockCopy(payload, 0, packet, sizeBuffer.Length, payload.Length);

            await _udpClient.SendAsync(packet, packet.Length, _serverUdpEndPoint);

            lock (_sentPacketLock)
            {
                ++_sentPacketCount;
            }
            
            Debug.Log($"Sent Rpc Packet To Server {ProtoSerializer.ConvertTimestampToString(data.Timestamp)}" +
                      $" {ProtoSerializer.ConvertUuidToGuid(data.Uuid).ToString()} : {data.Method}");
        }
        catch (Exception ex)
        {
            Debug.LogError($"send to failed: {ex.Message}");
        }
    }
    
    private async UniTask AsyncWriteByTcp(RpcPacket data)
    {
        if (!IsOnline)
            return;
        
        try
        {
            // Tcp Send
            byte[] sendPacket = ProtoSerializer.SerializeNetworkData(data);
            byte[] packetSize = BitConverter.GetBytes(sendPacket.Length);
            
            if(BitConverter.IsLittleEndian)
                Array.Reverse(packetSize);
            
            await _tcpStream.WriteAsync(packetSize, 0, packetSize.Length, CToken);
            await _tcpStream.WriteAsync(sendPacket, 0, sendPacket.Length, CToken);
        }
        catch (Exception e)
        {
            Debug.Log($"error tcp send: {e.Message}");
        }
    }
    
    private async UniTask TcpAsyncReadData()
    {
        try
        {
            uint netSize = 0;
            var sizeBuffer = new byte[sizeof(uint)]; // 4 bytes
            
            int bytesRead = await _tcpStream.ReadAsync(sizeBuffer, 0, sizeBuffer.Length, CToken);
            if (bytesRead == 0) // EOF connection closed
                DisconnectFromServer();
            
            // Convert the size from bytes to uint
            if (BitConverter.IsLittleEndian)
                Array.Reverse(sizeBuffer);
            
            netSize = BitConverter.ToUInt32(sizeBuffer, 0);
            var dataBuffer = new byte[netSize]; // netSize : 3 bytes
            
            // Read the rest of the data
            bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, CToken);
            
            if (bytesRead == 0) // EOF connection closed
                DisconnectFromServer();

            if (netSize > 128)
            {
                TcpAsyncReadData().Forget(); // Read the next data
                return;
            }
            
            // Deserialize the data
            RpcPacket data = ProtoSerializer.DeserializeNetworkData(dataBuffer);

            AsyncProcessRpc(data).Forget(); // Process the data asynchronously
            TcpAsyncReadData().Forget(); // Read the next data
        }
        catch (Exception ex)
        {
            Debug.Log($"Exception in TcpAsyncReadData: {ex.Message}");
        }
    }

    private async UniTask UdpAsyncReadData()
    {
        try
        {
            // Read the data from the stream
            UdpReceiveResult result = await _udpClient.ReceiveAsync();
            byte[] readData = result.Buffer;
            
            if (readData.Length < sizeof(ushort))
                return;
            
            // Read size of the payload
            var payloadSize = BitConverter.ToUInt16(readData, 0);
            
            // convert if little-endian system
            if (BitConverter.IsLittleEndian)
                payloadSize = (ushort)IPAddress.NetworkToHostOrder((short)payloadSize);
            
            // Check if the payload size is valid
            if (payloadSize == 0 || payloadSize > readData.Length - 2)
                return;
            
            // Extract the payload
            var payload = new byte[payloadSize];
            Buffer.BlockCopy(readData, 2, payload, 0, payloadSize);
            
            // Deserialize the data
             RpcPacket data = ProtoSerializer.DeserializeNetworkData(payload);
            AsyncProcessRpc(data).Forget(); // Process the data asynchronously
            UdpAsyncReadData().Forget();
            
            lock (_receivedPacketLock)
            {
                ++_receivedPacketCount;
            }
        }
        catch (Exception ex)
        {
            Debug.LogError($"Exception in ReadDataForInGameRpc: {ex.Message}");
        }

        await UniTask.Yield();
    }
    
    private async UniTask AsyncProcessRpc(RpcPacket data)
    {
        switch (data.Method)
        {
            case RpcMethod.Uuid:
                ConnectedUuid = ProtoSerializer.ConvertUuidToGuid(data.Uuid);
                Debug.Log($"Connected UUID: {ConnectedUuid.ToString()}");
                break;
            
            case RpcMethod.Ping:
                var pongPacket = new RpcPacket
                {
                    Method = RpcMethod.Pong,
                    Uuid = ProtoSerializer.SerializeUuid(ConnectedUuid)
                };

                AsyncWriteByTcp(pongPacket).Forget();
                break;
            
            
            case RpcMethod.Move:
                // Deserialize the data
                PositionData positionData = PositionData.Parser.ParseFrom(data.Data);
                
                var startPosition = new Vector3(positionData.X1, positionData.Y1, positionData.Z1);
                var targetPosition = new Vector3(positionData.X2, positionData.Y2, positionData.Z2);
                
                // Call SyncManager to sync the object position
                SyncManager.Instance.SyncObjectPosition(ProtoSerializer.ConvertUuidToGuid(data.Uuid), startPosition, targetPosition);
                break;
            
            case RpcMethod.PacketCount:
            case RpcMethod.Attack:
            case RpcMethod.DropItem:
            case RpcMethod.UseItem:
            case RpcMethod.UseSkill:
                Debug.Assert(false, "Not Implemented");
                break;
            
            case RpcMethod.Pong:
                Debug.Assert(false, "Not Client Method");
                break;
            
            case RpcMethod.Login:
            case RpcMethod.Register:
            case RpcMethod.Retrieve:
            case RpcMethod.Access:
            case RpcMethod.Reject:
            case RpcMethod.Logout:
                Debug.Assert(false, "Not Sync Method");
                break;
            
            case RpcMethod.None:
            case RpcMethod.InGameNone:
            default:
                Debug.Assert(false, "Invalid Method");
                break;
        }
        
        await UniTask.Yield();
    }
}