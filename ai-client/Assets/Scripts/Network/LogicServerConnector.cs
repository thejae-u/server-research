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
using UnityEditor.Search;
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
        private const uint MAX_PACKET_SIZE = 65536; // 64KB

        private int SentPacketCount
        {
            get
            {
                return Interlocked.CompareExchange(ref _sentPacketCount, 0, 0);
            }
        }
        private volatile int _sentPacketCount = 0;

        private int ReceivedPacketCount
        {
            get
            {
                return Interlocked.CompareExchange(ref _receivedPacketCount, 0, 0);
            }
        }
        private volatile int _receivedPacketCount = 0;

        public int LastRtt
        {
            get
            {
                return Interlocked.CompareExchange(ref _lastRtt, 0, 0);
            }

            private set
            {
                Interlocked.Exchange(ref _lastRtt, value);
            }
        }
        private volatile int _lastRtt = 0;
        private readonly List<int> _rttList = new();

        public int RttAverage 
        { 
            get 
            {
                return Interlocked.CompareExchange(ref _rttAverage, 0, 0);
            }

            set
            {
                Interlocked.Exchange(ref _rttAverage, value);
            }
        }
        private volatile int _rttAverage = 0;

        public int ErrorCount
        {
            get
            {
                return Interlocked.CompareExchange(ref _errorCount, 0, 0);
            }
        }
        private volatile int _errorCount = 0;
        public void IncrementErrorCount() { Interlocked.Increment(ref _errorCount); }

        public List<UserSimpleDto> Users => _currentGroupDto.PlayerList.ToList();
        private GroupDto _currentGroupDto = null;
        private AuthManager _authManager;

        public CancellationToken CToken => _globalCancellationToken.Token;
        private readonly CancellationTokenSource _globalCancellationToken = new();

        private Task _tcpReadTask;
        private Task _udpReadTask;

        private Task _sendRpcPacketByUdpTask;
        private readonly ConcurrentQueue<RpcPacket> _sendPacketQueueForUdp = new();

        private Task _sendRpcPacketByTcpTask;
        private readonly ConcurrentQueue<RpcPacket> _sendPacketQueueForTcp = new();

        private readonly List<Task> tasks = new();

        public float ErrorRate
        {
            get
            {
                if (ErrorCount == 0)
                {
                    return 0.0f;
                }

                return ErrorCount / (float)ReceivedPacketCount * 100.0f;
            }
        }

        public bool IsOnline
        {
            get 
            {
                return Interlocked.CompareExchange(ref _isOnline, 0, 0) == 1;
            }

            private set
            {
                Interlocked.Exchange(ref _isOnline, value ? 1 : 0);
            }
        }
        private volatile int _isOnline;

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
            _globalCancellationToken.Cancel();
            _globalCancellationToken.Dispose();

            foreach(var task in tasks)
            {
                task.Dispose();
            }

            DisconnectFromServer();
        }

        private void OnApplicationQuit()
        {
            _globalCancellationToken.Cancel();
            _globalCancellationToken.Dispose();

            foreach(var task in tasks)
            {
                task.Dispose();
            }

            DisconnectFromServer();
        }

        private void Update()
        {
            if (!IsOnline) 
                return;

            ProcessRpc();
        }

        public async Task<bool> TryConnectToServer(InRoomManager roomManager, GroupDto groupInfo, string ip, ushort port)
        {
            if (IsOnline)
            {
                Debug.Log($"already connected.");
                return false;
            }

            if (IsTryingToConnect)
            {
                Debug.Log($"already trying to connect.");
                return false;
            }

            Debug.Log($"NetworkManager TryConnectToServer");

            _currentGroupDto = groupInfo;
            Host = ip;
            Port = port;

            bool isConnected = await ConnectToServer(CToken);
            if (!isConnected)
            {
                IsTryingToConnect = false;
                return false;
            }

            return true;
        }

        private async Task<bool> ConnectToServer(CancellationToken ct)
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
                catch(OperationCanceledException)
                {
                    Debug.LogWarning("Cancelled");
                    return false;
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
            if (!await AsyncExchangeUdpPort(ct))
            {
                DisconnectFromServer();
                return false;
            }

            // Send user info and group info
            if (!await AsyncExchangeUserInfo(ct))
            {
                DisconnectFromServer();
                return false;
            }

            if (!await AsyncExchangeGroupInfo(ct))
            {
                DisconnectFromServer();
                return false;
            }

            return true;
        }

        public void StartGameTask()
        {
            Dispatcher.Enqueue(() => Debug.Log($"Called Start Task"));

            // prevent duplication 
            if (IsOnline)
                return;

            // internal state init
            IsOnline = true;
            IsTryingToConnect = false;

            // network statistics init
            _sentPacketCount = 0;
            _receivedPacketCount = 0;

            _tcpReadTask = Task.Run(() => TcpAsyncReadData(CToken), CToken); // Tcp read Task start
            _udpReadTask = Task.Run(() => UdpAsyncReadData(CToken), CToken); // Udp read Task start

            _sendRpcPacketByUdpTask = Task.Run(() => AsyncWriteByUdp(CToken), CToken); // rpc send by tcp Task start
            _sendRpcPacketByTcpTask = Task.Run(() => AsyncWriteByTcp(CToken), CToken); // rpc send by udp Task start

            // task List add
            tasks.Add(_tcpReadTask);
            tasks.Add(_udpReadTask);
            tasks.Add(_sendRpcPacketByUdpTask);
            tasks.Add(_sendRpcPacketByTcpTask);
        }

        private async Task<bool> AsyncExchangeUdpPort(CancellationToken ct)
        {
            try
            {
                var readPacketNetSize = new byte[4];

                // Read the size of the incoming data
                if(!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct))
                {
                    return false;
                }

                // Network to Host
                if (BitConverter.IsLittleEndian)
                    Array.Reverse(readPacketNetSize);

                var readPacketHostSize = BitConverter.ToUInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];

                // Read the rest of the data
                if(!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct))
                {
                    return false;
                }

                // Deserialize the data
                RpcPacket receiveUdpPort = RpcPacket.Parser.ParseFrom(readDataBuffer);

                // Network to Host
                var byteData = receiveUdpPort.Data.ToByteArray();
                if (byteData.Length != sizeof(ushort))
                {
                    Debug.LogError($"Invalid port data length: {byteData.Length}");
                    return false;
                }

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(byteData);

                ushort udpPort = BitConverter.ToUInt16(byteData, 0);
                Debug.Log($"Received Server Udp Port: {udpPort}");

                // Udp Socket Init
                _udpClient = new UdpClient(new IPEndPoint(IPAddress.Any, 0));
                const int SIO_UDP_CONNRESET = -1744830452;
                byte[] inValue = new byte[] { 0 };
                byte[] outValue = new byte[] { 0 };
                _udpClient.Client.IOControl(SIO_UDP_CONNRESET, inValue, outValue);

                // Endpoint set
                _clientUdpEndPoint = (IPEndPoint)_udpClient.Client.LocalEndPoint!;
                _serverUdpEndPoint = new IPEndPoint(IPAddress.Parse(Host), udpPort);
                Debug.Log($"Udp Client Created - {_serverUdpEndPoint.Address}:{_serverUdpEndPoint.Port}");

                // Send the UDP port to the server
                var clientPort = (ushort)_clientUdpEndPoint.Port;
                Debug.Log($"Send Port : {clientPort}");

                var netClientPort = BitConverter.GetBytes(clientPort);
                if (BitConverter.IsLittleEndian)
                    Array.Reverse(netClientPort);

                var sendClientPort = ByteString.CopyFrom(netClientPort);
                var sendUdpPortPacket = new RpcPacket
                {
                    Method = RpcMethod.UdpPort,
                    Data = sendClientPort,
                };

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
                var readPacketNetSize = new byte[4];
                if (!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct))
                {
                    return false;
                }

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(readPacketNetSize);

                // Read Data
                var readPacketHostSize = BitConverter.ToInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];
                if(!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct))
                {
                    return false;
                }

                // deserialize receive packet
                RpcPacket userInfoData = RpcPacket.Parser.ParseFrom(readDataBuffer);
                if (userInfoData.Method != RpcMethod.UserInfo)
                    return false;

                // send packet ready
                RpcPacket sendUserInfoData = new()
                {
                    Uid = _authManager.UserGuid.ToString(),
                    Method = RpcMethod.UserInfo,
                    Data = ByteString.CopyFrom(Encoding.UTF8.GetBytes(_authManager.Username))
                };

                byte[] sendData = sendUserInfoData.ToByteArray(); // Serialize rpc packet to byte[]
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
                var readPacketNetSize = new byte[4];
                if(!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct))
                {
                    return false;
                }

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(readPacketNetSize);

                var readPacketHostSize = BitConverter.ToInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];

                if(!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct))
                {
                    return false;
                }

                RpcPacket readPacket = RpcPacket.Parser.ParseFrom(readDataBuffer);
                if (readPacket.Method != RpcMethod.GroupInfo)
                    return false;

                var sendDtoString = JsonFormatter.Default.Format(_currentGroupDto);
                Debug.Log($"Send Dto String : {sendDtoString}");

                RpcPacket sendPacket = new()
                {
                    Method = RpcMethod.GroupInfo,
                    Uid = _authManager.UserGuid.ToString(),
                    Data = ByteString.CopyFrom(Encoding.UTF8.GetBytes(sendDtoString))
                };

                byte[] sendPacketData = sendPacket.ToByteArray(); // serialize rpc packet to byte[]
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

        public void EnqueueRpcPacketForUdp(RpcPacket sendPacket)
        {
            _sendPacketQueueForUdp.Enqueue(sendPacket);
        }

        private async Task AsyncWriteByUdp(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    if (!_sendPacketQueueForUdp.TryDequeue(out var data))
                    {
                        await Task.Yield();
                        continue;
                    }

                    data.Uid = _authManager.UserGuid.ToString();

                    byte[] payload = data.ToByteArray();
                    var payloadSize = (short)payload.Length;
                    byte[] sizeBuffer = BitConverter.GetBytes(payloadSize);

                    if (BitConverter.IsLittleEndian)
                        Array.Reverse(sizeBuffer);

                    var packet = new byte[sizeBuffer.Length + payload.Length];
                    Buffer.BlockCopy(sizeBuffer, 0, packet, 0, sizeBuffer.Length);
                    Buffer.BlockCopy(payload, 0, packet, sizeBuffer.Length, payload.Length);

                    await _udpClient.SendAsync(packet, packet.Length, _serverUdpEndPoint).ConfigureAwait(false);
                    Interlocked.Increment(ref _sentPacketCount); // sent packet count increment by atomic

                    Dispatcher.Enqueue(() => Debug.Log($"Sent Rpc Packet To Server {Utility.Util.ConvertTimestampToString(data.Timestamp)}" +
                              $" {data.Uid}: {data.Method}"));
                }
                catch (OperationCanceledException)
                {
                    Dispatcher.Enqueue(() => Debug.Log("Cancelled"));
                }
                catch (Exception ex)
                {
                    Dispatcher.Enqueue(() => Debug.LogError($"send failed: {ex.Message}"));
                }
            }
        }

        private void EnqueueRpcPacketForTcp(RpcPacket sendPacket)
        {
            _sendPacketQueueForTcp.Enqueue(sendPacket);
        }

        private async Task AsyncWriteByTcp(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    if(!_sendPacketQueueForTcp.TryDequeue(out var data))
                    {
                        await Task.Yield();
                        continue;
                    }

                    // Tcp Send
                    byte[] sendPacket = data.ToByteArray();
                    byte[] packetSize = BitConverter.GetBytes(sendPacket.Length);

                    if (BitConverter.IsLittleEndian)
                        Array.Reverse(packetSize);

                    await _tcpStream.WriteAsync(packetSize, 0, packetSize.Length, ct).ConfigureAwait(false);
                    await _tcpStream.WriteAsync(sendPacket, 0, sendPacket.Length, ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    return;
                }
                catch (Exception e)
                {
                    Dispatcher.Enqueue(() => Debug.Log($"error tcp send: {e.Message}"));
                    return;
                }
            }
        }

        private async Task TcpAsyncReadData(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    var sizeBuffer = new byte[sizeof(uint)]; // 4 bytes read size buffer

                    // Read Size first for data receive
                    if(!await ReadTcpExactlyAsync(_tcpStream, sizeBuffer, sizeBuffer.Length, ct).ConfigureAwait(false))
                    {
                        Dispatcher.Enqueue(() => Debug.Log($"failed to read packet size."));
                        DisconnectFromServer();
                        return;
                    }

                    // Convert the size from bytes to uint
                    if (BitConverter.IsLittleEndian)
                        Array.Reverse(sizeBuffer);

                    uint netSize = BitConverter.ToUInt32(sizeBuffer, 0);

                    if(netSize == 0 || netSize > MAX_PACKET_SIZE)
                    {
                        Dispatcher.Enqueue(() => Debug.LogError($"Invalid Packet Size received {netSize}."));
                        DisconnectFromServer();
                        return;
                    }

                    var dataBuffer = new byte[netSize]; // real data buffer sizeof netSize

                    // Read the rest of the data
                    if(!await ReadTcpExactlyAsync(_tcpStream, dataBuffer, dataBuffer.Length, ct).ConfigureAwait(false))
                    {
                        Dispatcher.Enqueue(() => Debug.LogError($"failed to read packet data."));
                        DisconnectFromServer();
                        return;
                    }

                    EnqueueInternalProcess(dataBuffer);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Dispatcher.Enqueue(() => Debug.LogError($"TCP read size error: {ex.Message}"));
                }
            }
        }

        private async Task<bool> ReadTcpExactlyAsync(NetworkStream stream, byte[] buffer, int length, CancellationToken ct)
        {
            int totalBytesRead = 0;
            while(totalBytesRead < length)
            {
                try
                {
                    int bytesRead = await stream.ReadAsync(buffer, totalBytesRead, length - totalBytesRead, ct).ConfigureAwait(false); // 총 길이 - 현재 길이 만큼 더 받음
                    if(bytesRead == 0)
                    {
                        Dispatcher.Enqueue(() => Debug.LogWarning("(EOF) disconnected"));
                        return false;
                    }

                    totalBytesRead += bytesRead;
                }
                catch(OperationCanceledException)
                {
                    Dispatcher.Enqueue(() => Debug.Log($"cancelled"));
                    return false;
                }
                catch(Exception ex)
                {
                    Dispatcher.Enqueue(() => Debug.LogError($"error ReadTcpExactly: {ex.Message}"));
                    return false;
                }
            }

            return true;
        }

        private async Task UdpAsyncReadData(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    // Read the data from the stream
                    UdpReceiveResult result = await _udpClient.ReceiveAsync().ConfigureAwait(false);
                    byte[] readData = result.Buffer;

                    // Error 시 버림
                    if (readData.Length < sizeof(ushort))
                    {
                        IncrementErrorCount();
                        continue;
                    }

                    // Read size of the payload
                    short netOrderSize = BitConverter.ToInt16(readData, 0);

                    // convert if little-endian system
                    ushort payloadSize = (ushort)IPAddress.NetworkToHostOrder(netOrderSize);

                    // Check if the payload size is valid
                    if (netOrderSize == 0 || netOrderSize > readData.Length - 2)
                    {
                        IncrementErrorCount();
                        continue;
                    }

                    // Extract the payload
                    var payload = new byte[payloadSize];
                    Buffer.BlockCopy(readData, 2, payload, 0, payloadSize);

                    EnqueueInternalProcess(payload);
                    Interlocked.Increment(ref _receivedPacketCount); // Increment received packet count by atomic
                }
                catch (OperationCanceledException)
                {
                    Dispatcher.Enqueue(() => Debug.Log($"UDP loop canceled"));
                    break;
                }
                catch (ObjectDisposedException ex)
                {
                    Dispatcher.Enqueue(() => Debug.LogError($"{ex.HResult} : error object dis {ex.Message}"));
                    break;
                }
                catch (SocketException ex)
                {
                    Dispatcher.Enqueue(() => Debug.LogError($"{ex.SocketErrorCode} : error socket {ex.Message}"));
                    break;
                }
                catch (Exception ex)
                {
                    Dispatcher.Enqueue(() => Debug.LogError($"UDP error: {ex.Message}"));
                    break;
                }
            }
        }

        private void EnqueueInternalProcess(byte[] data)
        {
            Dispatcher.Enqueue(() => Debug.Log($"data enqueued"));
            _processQueue.Enqueue(data);
        }

        private byte[] DequeueInternalProcess()
        {
            if (!_processQueue.TryDequeue(out var data))
            {
                return null;
            }

            Dispatcher.Enqueue(() => Debug.Log($"data dequeued"));
            return data;
        }

        private void ProcessRpc()
        {
            var nextProcess = DequeueInternalProcess();

            if (nextProcess is null)
            {
                Debug.Log($"next process is null");
                return;
            }

            var data = RpcPacket.Parser.ParseFrom(nextProcess);
            if (data is null)
            {
                Debug.Log($"Data is null");
                return;
            }

            switch (data.Method)
            {
                case RpcMethod.Ping:
                    var pongPacket = new RpcPacket
                    {
                        Method = RpcMethod.Pong,
                        Uid = _authManager.UserGuid.ToString()
                    };

                    EnqueueRpcPacketForTcp(pongPacket);
                    break;

                case RpcMethod.MoveStart:
                case RpcMethod.MoveStop:
                case RpcMethod.Move:
                    Debug.Log($"Move data process");
                    // Deserialize the data
                    MoveData moveData = MoveData.Parser.ParseFrom(data.Data);

                    // Call SyncManager to sync the object position
                    SyncManager.Instance.SyncObjectPosition(Guid.Parse(data.Uid), moveData);
                    break;

                case RpcMethod.LastRtt:
                    LastRtt = int.Parse(data.Data.ToStringUtf8());
                    _rttList.Add(LastRtt);

                    RttAverage = (int)_rttList.Average(x => x);
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