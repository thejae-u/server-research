using System;
using System.Net;
using UnityEngine;
using System.Net.Sockets;
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
    [SerializeField] private NetworkInputAction _inputAction;
    public Guid ConnectedUuid { get; private set; }
    
    private TcpClient _tcpClient;
    private UdpClient _udpClient;
    private IPEndPoint _udpEndPoint;
    private NetworkStream _tcpStream;
    private int _maxRetries = 5;

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
            
            
            await _tcpClient.ConnectAsync(_serverIp, 53200);
            _tcpStream = _tcpClient.GetStream();
            
            // Get Udp Port
            await AsyncReceiveUdpPort();
            
            // RTT Check
            await AsyncPing();
            
            // Receive UUID
            await AsyncReceiveUuid();
            
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

        AsyncReadSizeForInGameRpc().Forget();
        IsTryingToConnect = false;
    }

    private async UniTask AsyncReceiveUdpPort()
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
            Debug.Log($"UDP Port: {udpPort}");

            _udpClient = new UdpClient();
            _udpEndPoint = new IPEndPoint(IPAddress.Parse(_serverIp), udpPort);
            Debug.Log($"Udp Client Created - {_udpEndPoint.Address}:{_udpEndPoint.Port}");
        }
        catch (Exception ex)
        {
            Debug.LogError($"error in AsyncReceiveUdpPort: {ex.Message}");
        }
        
        await UniTask.Yield();
    }

    private async UniTask AsyncPing()
    {
        try
        {
            var pingPacketSize = new byte[4];
            
            Util.StartStopwatch();
            int readSize = await _tcpStream.ReadAsync(pingPacketSize, 0, pingPacketSize.Length, CToken);
            if (readSize == 0)
            {
                Debug.Log($"Ping Packet Size is Empty");
                return;
            }
            
            // Convert the size from bytes to uint
            if (BitConverter.IsLittleEndian)
                Array.Reverse(pingPacketSize);
            var dataSize = BitConverter.ToUInt32(pingPacketSize, 0);
            
            var dataBuffer = new byte[dataSize];
            // Read the rest of the data
            readSize = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, CToken);
            
            if (readSize == 0) // EOF connection closed
            {
                Debug.Log($"Ping Packet is Empty");
                return;
            }
            
            var pongPacket = new RpcPacket
            {
                Method = RpcMethod.Pong,
            };

            byte[] sendPongPacket = pongPacket.ToByteArray();
            byte[] pongSize = BitConverter.GetBytes(sendPongPacket.Length);
            
            // Convert to little-endian if necessary
            if (BitConverter.IsLittleEndian)
                Array.Reverse(pongSize);
            
            // Send ping packet
            await _tcpStream.WriteAsync(pongSize, 0, pongSize.Length);
            await _tcpStream.WriteAsync(sendPongPacket, 0, sendPongPacket.Length);
            Debug.Log($"Ping RTT checked by client: {Util.EndStopwatch()} ms");
        }
        catch (Exception ex)
        {
            Debug.LogError($"Ping failed: {ex.Message}");
            DisconnectFromServer();
        }
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
            
            AsyncProcessRpc(ProtoSerializer.DeserializeNetworkData(dataBuffer)).Forget(); // Process the data asynchronously
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

        Debug.Log("disconnected.");
    }
    
    public async UniTask AsyncWriteRpcPacket(RpcPacket data)
    {
        if (!IsOnline)
            return;

        try
        {
            data.Uuid = ProtoSerializer.SerializeUuid(ConnectedUuid);
            
            // Serialize the data to a string 
            byte[] sendData = ProtoSerializer.SerializeNetworkData(data);

            // send size first
            var dataSize = (uint)sendData.Length;
            byte[] sizeBytes = BitConverter.GetBytes(dataSize);
            
            // Convert to little-endian if necessary
            if (BitConverter.IsLittleEndian)
                Array.Reverse(sizeBytes);

            // Send size
            await _udpClient.SendAsync(sizeBytes, sizeBytes.Length, _udpEndPoint);

            // Send data
            await _udpClient.SendAsync(sendData, sendData.Length, _udpEndPoint);
            
            Debug.Log($"Sent Rpc Packet To Server {ProtoSerializer.ConvertTimestampToString(data.Timestamp)}" +
                      $" {ProtoSerializer.ConvertUuidToGuid(data.Uuid).ToString()} {data.Method}");
        }
        catch (Exception ex)
        {
            Debug.LogError($"send to failed: {ex.Message}");
        }
    }


    private async UniTask AsyncReadSizeForInGameRpc()
    {
        if (!IsOnline)
            return;

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

            // Read the rest of the data
            ReadDataForInGameRpc(dataBuffer);
        }
        catch (Exception ex)
        {
            Debug.LogError($"Exception in AsyncReadSizeForInGameRpc: {ex.Message}");
            DisconnectFromServer();
            return;
        }

        _netSize = 0;
        await UniTask.Yield();
    }

    private void ReadDataForInGameRpc(byte[] dataBuffer)
    {
        try
        {
            // Read the data from the stream
            int bytesRead = _tcpStream.Read(dataBuffer, 0, dataBuffer.Length);
            if (bytesRead == 0) // EOF connection closed
            {
                Debug.Log($"ReadDataForInGameRpc - EOF connection closed");
                return;
            }

            // Deserialize the data
            RpcPacket data = ProtoSerializer.DeserializeNetworkData(dataBuffer);
            AsyncProcessRpc(data).Forget(); // Process the data asynchronously
            AsyncReadSizeForInGameRpc().Forget();
        }
        catch (Exception ex)
        {
            Debug.LogError($"Exception in ReadDataForInGameRpc: {ex.Message}");
        }
    }
    
    private async UniTask AsyncProcessRpc(RpcPacket data)
    {
        switch (data.Method)
        {
            case RpcMethod.Uuid:
                ConnectedUuid = ProtoSerializer.ConvertUuidToGuid(data.Uuid);
                Debug.Log($"Connected UUID: {ConnectedUuid.ToString()}");
                break;
            
            case RpcMethod.Move:
                // Deserialize the data
                PositionData positionData = PositionData.Parser.ParseFrom(data.Data);
                
                var startPosition = new Vector3(positionData.X1, positionData.Y1, positionData.Z1);
                var targetPosition = new Vector3(positionData.X2, positionData.Y2, positionData.Z2);
                
                // Call SyncManager to sync the object position
                SyncManager.Instance.SyncObjectPosition(ProtoSerializer.ConvertUuidToGuid(data.Uuid), startPosition, targetPosition);
                break;
            
            case RpcMethod.Attack:
            case RpcMethod.DropItem:
            case RpcMethod.UseItem:
            case RpcMethod.UseSkill:
                Debug.Assert(false, "Not Implemented");
                break;
            
            case RpcMethod.Ping:
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