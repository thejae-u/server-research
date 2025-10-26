using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;

using System.Net;
using System.Net.Sockets;
using System.Text;

using System.Threading;
using System.Threading.Tasks;

using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using NetworkData;
using UnityEngine;

namespace Network
{
    public class LogicServerConnector : Singleton<LogicServerConnector>
    {
        public string Host { get; private set; }
        public ushort Port { get; private set; }

        public Action disconnectAction;

        private TcpClient _tcpClient;
        private UdpClient _udpClient;
        private IPEndPoint _serverUdpEndPoint;
        private IPEndPoint _clientUdpEndPoint;
        private NetworkStream _tcpStream;

        private readonly ConcurrentQueue<byte[]> _processQueue = new();

        private int _maxRetries = 5;

        private readonly object _sentPacketLock = new();
        private uint _sentPacketCount = 0;

        private readonly object _receivedPacketLock = new();
        private uint _receivedPacketCount = 0;

        private uint _lastRtt = 0;
        public uint LastRtt => _lastRtt;
        private List<uint> _rttList = new();
        public uint RttAverage { get; private set; }

        public uint ErrorCount { get; set; }
        private object _errorCountLock = new();

        private GroupDto _currentGroupDto = null;
        private AuthManager _authManager;

        private Task _tcpReadTask;
        private Task _udpReadTask;

        public float ErrorRate
        {
            get
            {
                lock (_errorCountLock)
                {
                    if (ErrorCount == 0)
                    {
                        return 0.0f;
                    }

                    return ErrorCount / (float)_receivedPacketCount * 100.0f;
                }
            }
        }

        private readonly CancellationTokenSource _cancellationTokenSource = new();
        private CancellationToken _globalCancellationToken => _cancellationTokenSource.Token;

        public bool IsOnline { get; private set; }
        public bool IsSendPacketOn { get; private set; }
        private bool IsTryingToConnect { get; set; }

        private void Start()
        {
            _authManager = AuthManager.Instance;

            IsSendPacketOn = false;
            IsOnline = false;
            IsTryingToConnect = false;
        }

        private void OnDestroy()
        {
            _cancellationTokenSource.Cancel();
            _cancellationTokenSource.Dispose();
            DisconnectFromServer();
        }

        private void Update()
        {
            ProcessRpc();
        }

        public async Task TryConnectToServer(InRoomManager roomManager, GroupDto groupInfo, string ip, ushort port)
        {
            if (IsOnline)
            {
                Debug.Log($"already connected.");
                return;
            }

            if (IsTryingToConnect)
            {
                Debug.Log($"already trying to connect.");
                return;
            }

            Debug.Log($"NetworkManager TryConnectToServer");

            _currentGroupDto = groupInfo;
            Host = ip;
            Port = port;

            var connectTask = await ConnectToServer();
            if (!connectTask)
            {
                roomManager.InitByFailedConnection();
                IsTryingToConnect = false;
                return;
            }

            // Scene Change
            SceneController.Instance.LoadSceneAsync(SceneController.EScene.GameScene);
        }

        private async Task<bool> ConnectToServer()
        {
            if (IsOnline)
                return false;

            IsTryingToConnect = true;
            _tcpClient = new TcpClient();

            Debug.Log("Connecting to server Start");

            for (int i = 0; i <= _maxRetries; ++i)
            {
                try
                {
                    await _tcpClient.ConnectAsync(Host, Port);
                    break;
                }
                catch (SocketException ex)
                {
                    Debug.Log($"서버 연결 실패: {ex.Message}");

                    if (i < _maxRetries)
                    {
                        Debug.Log($"재시도...{_maxRetries - i}");
                    }
                    else
                    {
                        Debug.Log($"서버 연결 실패: {ex.Message}");
                        _currentGroupDto = null;
                        return false;
                    }
                }
                catch (Exception ex)
                {
                    Debug.LogError($"Unknown Error: {ex.Message}");
                    _currentGroupDto = null;
                    return false;
                }
            }

            _tcpStream = _tcpClient.GetStream();

            // Get Udp Port
            if (!await AsyncExchangeUdpPort(_globalCancellationToken))
            {
                DisconnectFromServer();
                return false;
            }

            // Send user info and group info
            if (!await AsyncExchangeUserInfo(_globalCancellationToken))
            {
                DisconnectFromServer();
                return false;
            }

            if (!await AsyncExchangeGroupInfo(_globalCancellationToken))
            {
                DisconnectFromServer();
                return false;
            }

            _sentPacketCount = 0;
            _receivedPacketCount = 0;

            IsOnline = true;

            _tcpReadTask = TcpAsyncReadData(_globalCancellationToken);
            _udpReadTask = UdpAsyncReadData(_globalCancellationToken);

            IsTryingToConnect = false;

            // Scene Change Call Here

            return true;
        }

        private async Task<bool> AsyncExchangeUdpPort(CancellationToken ct)
        {
            try
            {
                var udpPortPacketSize = new byte[4];
                // Read the size of the incoming data
                int bytesRead = await _tcpStream.ReadAsync(udpPortPacketSize, 0, udpPortPacketSize.Length, ct);

                if (bytesRead == 0)
                    return false;

                // Convert the size from bytes to uint
                if (BitConverter.IsLittleEndian)
                    Array.Reverse(udpPortPacketSize);

                var dataSize = BitConverter.ToUInt32(udpPortPacketSize, 0);
                var dataBuffer = new byte[dataSize];

                // Read the rest of the data
                bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, ct);

                // EOF connection closed
                if (bytesRead == 0)
                    return false;

                // Deserialize the data
                RpcPacket udpPortData = ProtoSerializer.DeserializeNetworkData(dataBuffer);

                // Convert string to ushort (Port)
                var byteData = Encoding.UTF8.GetBytes(udpPortData.Data);
                if (!BitConverter.IsLittleEndian)
                    Array.Reverse(byteData);

                ushort udpPort = BitConverter.ToUInt16(byteData, 0);

                // Endpoint Setting
                _udpClient = new UdpClient(new IPEndPoint(IPAddress.Any, 0));
                _clientUdpEndPoint = (IPEndPoint)_udpClient.Client.LocalEndPoint!;
                _serverUdpEndPoint = new IPEndPoint(IPAddress.Parse(Host), udpPort);
                Debug.Log($"Udp Client Created - {_serverUdpEndPoint.Address}:{_serverUdpEndPoint.Port}");

                // Send the UDP port to the server
                var clientPort = (ushort)_clientUdpEndPoint.Port;
                var sendUdpPortPacket = new RpcPacket
                {
                    Method = RpcMethod.UdpPort,
                    Data = clientPort.ToString(),
                };

                Debug.Log($"Send Udp Port little endian - {clientPort}");

                // convert client port for send
                byte[] sendUdpPortPacketData = sendUdpPortPacket.ToByteArray();
                byte[] udpPortSize = BitConverter.GetBytes(sendUdpPortPacketData.Length);
                if (BitConverter.IsLittleEndian)
                    Array.Reverse(udpPortSize);

                // Send the UDP port packet
                await _tcpStream.WriteAsync(udpPortSize, 0, udpPortSize.Length, ct);
                await _tcpStream.WriteAsync(sendUdpPortPacketData, 0, sendUdpPortPacketData.Length, ct);
                Debug.Log($"Exchanged Udp Port completed");
            }
            catch (OperationCanceledException)
            {
                Debug.Log($"Cancelled");
                return false;
            }
            catch (Exception ex)
            {
                Debug.LogError($"Exchange failed: {ex.Message}");
                return false;
            }

            return true;
        }

        private async Task<bool> AsyncExchangeUserInfo(CancellationToken ct)
        {
            try
            {
                // Read Size First
                var userInfoPacketSize = new byte[4];
                int bytesRead = await _tcpStream.ReadAsync(userInfoPacketSize, 0, userInfoPacketSize.Length, ct);

                if (bytesRead == 0)
                    return false;

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(userInfoPacketSize);

                // Read Data
                var dataSize = BitConverter.ToInt32(userInfoPacketSize, 0);
                var dataBuffer = new byte[dataSize];
                bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, ct);

                if (bytesRead == 0)
                    return false;

                // deserialize receive packet
                RpcPacket userInfoData = ProtoSerializer.DeserializeNetworkData(dataBuffer);
                if (userInfoData.Method != RpcMethod.UserInfo)
                    return false;

                // send packet ready
                RpcPacket sendUserInfoData = new()
                {
                    Uid = _authManager.UserGuid.ToString(),
                    Method = RpcMethod.UserInfo,
                    Data = _authManager.Username
                };

                byte[] sendData = ProtoSerializer.SerializeNetworkData(sendUserInfoData);
                byte[] sendDataSize = BitConverter.GetBytes(sendData.Length);

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(sendDataSize);

                await _tcpStream.WriteAsync(sendDataSize, 0, sendDataSize.Length, ct);
                await _tcpStream.WriteAsync(sendData, 0, sendData.Length, ct);
                Debug.Log($"Exchange UserInfo Complete");
            }
            catch (OperationCanceledException)
            {
                Debug.Log($"Cancelled");
                return false;
            }
            catch (Exception ex)
            {
                Debug.LogError($"Network Error {ex.Message}");
                return false;
            }

            return true;
        }

        private async Task<bool> AsyncExchangeGroupInfo(CancellationToken ct)
        {
            try
            {
                var readPacketSize = new byte[4];
                int readBytes = await _tcpStream.ReadAsync(readPacketSize, 0, readPacketSize.Length, ct);

                if (readBytes == 0)
                    return false;

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(readPacketSize);

                var readDataSize = BitConverter.ToInt32(readPacketSize, 0);
                var readBuffer = new byte[readDataSize];

                readBytes = await _tcpStream.ReadAsync(readBuffer, 0, readBuffer.Length, ct);
                if (readBytes == 0)
                    return false;

                RpcPacket readPacket = ProtoSerializer.DeserializeNetworkData(readBuffer);
                if (readPacket.Method != RpcMethod.GroupInfo)
                    return false;

                var sendDtoString = JsonFormatter.Default.Format(_currentGroupDto);

                Debug.Log($"Send Dto String : {sendDtoString}");

                RpcPacket sendPacket = new()
                {
                    Method = RpcMethod.GroupInfo,
                    Uid = _authManager.UserGuid.ToString(),
                    Data = sendDtoString,
                };

                byte[] sendPacketData = ProtoSerializer.SerializeNetworkData(sendPacket);
                byte[] sendPacketSize = BitConverter.GetBytes(sendPacketData.Length);

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(sendPacketSize);

                await _tcpStream.WriteAsync(sendPacketSize, 0, sendPacketSize.Length, ct);
                await _tcpStream.WriteAsync(sendPacketData, 0, sendPacketData.Length, ct);
                Debug.Log($"Exchange Complete");

                return true;
            }
            catch (OperationCanceledException)
            {
                Debug.Log("Cancelled");
                return false;
            }
            catch (Exception ex)
            {
                Debug.LogError($"Network Error: {ex.Message}");
                return false;
            }
        }

        private void DisconnectFromServer()
        {
            if (!IsOnline)
                return;

            _currentGroupDto = null;

            _tcpStream?.Close();
            _tcpClient?.Close();
            IsOnline = false;
            IsSendPacketOn = false;

            disconnectAction?.Invoke();

            Debug.Log("disconnected.");
        }

        public async Task AsyncWriteRpcPacket(RpcPacket data)
        {
            if (!IsOnline)
                return;

            try
            {
                data.Uid = _authManager.UserGuid.ToString();

                byte[] payload = ProtoSerializer.SerializeNetworkData(data);
                var payloadSize = (short)payload.Length;
                byte[] sizeBuffer = BitConverter.GetBytes(payloadSize);

                if (BitConverter.IsLittleEndian)
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
                          $" {ProtoSerializer.ConvertUuidToGuid(data.Uid)}: {data.Method}");
            }
            catch (Exception ex)
            {
                Debug.LogError($"send to failed: {ex.Message}");
            }
        }

        private async Task AsyncWriteByTcp(RpcPacket data, CancellationToken ct)
        {
            if (!IsOnline)
                return;

            try
            {
                // Tcp Send
                byte[] sendPacket = ProtoSerializer.SerializeNetworkData(data);
                byte[] packetSize = BitConverter.GetBytes(sendPacket.Length);

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(packetSize);

                await _tcpStream.WriteAsync(packetSize, 0, packetSize.Length, ct);
                await _tcpStream.WriteAsync(sendPacket, 0, sendPacket.Length, ct);
            }
            catch (OperationCanceledException)
            {
                return;
            }
            catch (Exception e)
            {
                Debug.Log($"error tcp send: {e.Message}");
                return;
            }
        }

        private async Task TcpAsyncReadData(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested || IsOnline)
            {
                var sizeBuffer = new byte[sizeof(uint)]; // 4 bytes
                int bytesRead = 0;

                try
                {
                    // Read Size first for data receive
                    bytesRead = await _tcpStream.ReadAsync(sizeBuffer, 0, sizeBuffer.Length, ct);

                    // EOF connection closed
                    if (bytesRead == 0)
                    {
                        DisconnectFromServer();
                        break;
                    }

                    // Convert the size from bytes to uint
                    if (BitConverter.IsLittleEndian)
                        Array.Reverse(sizeBuffer);

                    uint netSize = BitConverter.ToUInt32(sizeBuffer, 0);

                    if (netSize > 128)
                        continue;

                    var dataBuffer = new byte[netSize]; // netSize : 3 bytes

                    // Read the rest of the data
                    bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, ct);

                    // EOF connection closed
                    if (bytesRead == 0)
                    {
                        DisconnectFromServer();
                        break;
                    }

                    EnqueueProcess(dataBuffer);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Debug.LogError($"TCP read size error: {ex.Message}");
                }
            }
        }

        private async Task UdpAsyncReadData(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested && IsOnline)
            {
                // Read the data from the stream
                UdpReceiveResult result;

                try
                {
                    result = await _udpClient.ReceiveAsync();
                    byte[] readData = result.Buffer;

                    // Error 시 버림
                    if (readData.Length < sizeof(ushort))
                    {
                        lock (_errorCountLock)
                        {
                            ErrorCount++;
                        }

                        continue;
                    }

                    // Read size of the payload
                    var payloadSize = BitConverter.ToUInt16(readData, 0);

                    // convert if little-endian system
                    if (BitConverter.IsLittleEndian)
                        payloadSize = (ushort)IPAddress.NetworkToHostOrder((short)payloadSize);

                    // Check if the payload size is valid
                    if (payloadSize == 0 || payloadSize > readData.Length - 2)
                    {
                        lock (_errorCountLock)
                        {
                            ErrorCount++;
                        }

                        continue;
                    }

                    // Extract the payload
                    var payload = new byte[payloadSize];
                    Buffer.BlockCopy(readData, 2, payload, 0, payloadSize);

                    EnqueueProcess(payload);

                    lock (_receivedPacketLock)
                    {
                        ++_receivedPacketCount;
                    }
                }
                catch (OperationCanceledException)
                {
                    Debug.Log($"UDP loop canceled");
                    break;
                }
                catch (Exception ex)
                {
                    Debug.LogError($"UDP error: {ex.Message}");
                }
            }
        }

        private void EnqueueProcess(byte[] data)
        {
            _processQueue.Enqueue(data);
        }

        private byte[] DequeueProcess()
        {
            if (!_processQueue.TryDequeue(out var data))
            {
                return null;
            }

            return data;
        }

        private void ProcessRpc()
        {
            var nextProcess = DequeueProcess();

            if (nextProcess == null)
                return;

            var data = ProtoSerializer.DeserializeNetworkData(nextProcess);
            if (data == null)
                return;

            switch (data.Method)
            {
                case RpcMethod.Ping:
                    var pongPacket = new RpcPacket
                    {
                        Method = RpcMethod.Pong,
                        Uid = _authManager.ToString()
                    };

                    var task = AsyncWriteByTcp(pongPacket, _globalCancellationToken);
                    break;

                case RpcMethod.MoveStart:
                case RpcMethod.MoveStop:
                case RpcMethod.Move:
                    // Deserialize the data
                    MoveData moveData = MoveData.Parser.ParseFrom(Encoding.UTF8.GetBytes(data.Data));

                    // Call SyncManager to sync the object position
                    SyncManager.Instance.SyncObjectPosition(ProtoSerializer.ConvertUuidToGuid(data.Uid), moveData);
                    break;

                case RpcMethod.LastRtt:
                    byte[] rttData = Encoding.UTF8.GetBytes(data.Data);
                    _lastRtt = uint.Parse(Encoding.ASCII.GetString(rttData));
                    _rttList.Add(_lastRtt);
                    RttAverage = (uint)_rttList.Average(x => x);
                    break;

                case RpcMethod.PacketCount:
                case RpcMethod.NetworkNone:
                    Debug.Assert(false, "Not Implemented");
                    break;

                case RpcMethod.Pong:
                    Debug.Assert(false, "Not Client Method");
                    break;

                case RpcMethod.UdpPort:
                    Debug.Assert(false, "Not Sync Method");
                    break;

                // Network Initialize
                case RpcMethod.UserInfo:
                case RpcMethod.GroupInfo:
                case RpcMethod.InGameNone:
                default:
                    Debug.Assert(false, "Invalid Method");
                    break;
            }
        }
    }
}