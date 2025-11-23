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
using NetworkData; // UserSimpleDto, GroupDto가 포함되어 있다고 가정합니다.
using UnityEngine;
using UnityEngine.SceneManagement;

namespace Network
{
    public class ManualConnector : Singleton<ManualConnector>
    {
        public string Host { get; private set; }
        public ushort Port { get; private set; }

        public Action disconnectAction;

        private TcpClient _tcpClient;
        private UdpClient _udpClient;
        private IPEndPoint _serverUdpEndPoint;
        private IPEndPoint _clientUdpEndPoint;
        private NetworkStream _tcpStream;

        private readonly ConcurrentQueue<byte[]> _rawQueue = new();

        private Task _parsingRawPacketTask;
        private readonly ConcurrentQueue<Tuple<RpcPacket, object>> _readyQueue = new();

        private int _maxRetries = 5;
        private const uint MAX_PACKET_SIZE = 65536; // 64KB

        private readonly byte[] _receiveBufferHeader = new byte[sizeof(uint)];
        private readonly byte[] _receiveBufferData = new byte[MAX_PACKET_SIZE];

        public int SentPacketCount
        {
            get
            {
                return Interlocked.CompareExchange(ref _sentPacketCount, 0, 0);
            }
        }
        private volatile int _sentPacketCount = 0;

        public int ReceivedPacketCount
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

        // Manual User infos
        public Guid UserId { get; private set; }
        public string Name { get; private set; }

        // make randomly
        public List<UserSimpleDto> Users => _currentGroupDto.PlayerList.ToList();
        private GroupDto _currentGroupDto = null;

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

        private MainThreadDispatcher _dispatcher;

        private void Start()
        {
            UserId = Guid.NewGuid();
            Name = $"Player_{UserId.ToString().Substring(0, 6)}"; // 랜덤 UID 기반 이름

            _dispatcher = MainThreadDispatcher.Instance;
            IsSendPacketOn = false;

            IsOnline = false;
            IsTryingToConnect = false;
        }

        private void OnDestroy()
        {
            DisconnectFromServer();
        }

        private void OnApplicationQuit()
        {
            DisconnectFromServer();
        }

        private void OnEnable()
        {
            disconnectAction += DisconnectFromServer;
        }

        private void OnDisable()
        {
            disconnectAction -= DisconnectFromServer;
        }

        private void Update()
        {
            if (!IsOnline)
            {
                // [수정됨] F1 키로 연결 시도 로직 구현
                if (Input.GetKeyDown(KeyCode.F1))
                {
                    // Try Connect
                    Debug.Log("F1 키 입력: 서버 연결을 시도합니다...");

                    // 1. 고정된 Group ID 정의
                    string fixedGroupId = "a1b2c3d4-0000-0000-0000-1234567890ab"; // 예시용 고정 ID

                    // 2. 현재 유저 정보 생성 (Start에서 생성된 랜덤 UID와 이름 사용)
                    var currentUser = new UserSimpleDto
                    {
                        Uid = UserId.ToString(),
                        Username = Name
                    };

                    // 3. 서버에 전달할 GroupDto 생성
                    var groupInfo = new GroupDto
                    {
                        GroupId = fixedGroupId,
                        PlayerList = { currentUser } // Protobuf RepeatedField에 추가
                    };

                    // 5. 서버 연결 시도 (예시 IP 및 포트)
                    string hostIp = "127.0.0.1"; // 로컬 호스트
                    ushort hostPort = 53200; // 예시 포트

                    // 비동기 메서드 호출 (Update에서 await 불가능하므로 Task 실행)
                    _ = TryConnectToServer(groupInfo, hostIp, hostPort);
                }
                return;
            }

            ProcessRpc();
        }

        public async Task<bool> TryConnectToServer(GroupDto groupInfo, string ip, ushort port)
        {
            if (IsOnline)
            {
                _dispatcher.Enqueue(() => Debug.Log($"already connected."));
                return false;
            }

            if (IsTryingToConnect)
            {
                _dispatcher.Enqueue(() => Debug.Log($"already trying to connect."));
                return false;
            }

            _dispatcher.Enqueue(() => Debug.Log($"NetworkManager TryConnectToServer"));

            _currentGroupDto = groupInfo;
            Host = ip;
            Port = port;

            bool isConnected = await ConnectToServer(destroyCancellationToken);
            if (!isConnected)
            {
                IsTryingToConnect = false;
                return false;
            }

            StartGameTask();
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
                    _dispatcher.Enqueue(() => Debug.Log($"서버 연결 실패: {ex.Message}"));

                    if (i < _maxRetries)
                    {
                        _dispatcher.Enqueue(() => Debug.Log($"재시도...{_maxRetries - i}"));
                    }
                    else
                    {
                        _dispatcher.Enqueue(() => Debug.Log($"서버 연결 실패: {ex.Message}"));
                        _currentGroupDto = null;
                        return false;
                    }
                }
                catch (OperationCanceledException)
                {
                    Debug.LogWarning("Cancelled");
                    return false;
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"Unknown Error: {ex.Message}"));
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
            _dispatcher.Enqueue(() => Debug.Log($"Called start Task"));

            // prevent duplication 
            if (IsOnline)
                return;

            // internal state init
            IsOnline = true;
            IsTryingToConnect = false;

            // network statistics init
            _sentPacketCount = 0;
            _receivedPacketCount = 0;

            _tcpReadTask = Task.Run(() => TcpAsyncReadData(destroyCancellationToken), destroyCancellationToken); // Tcp read Task start
            _udpReadTask = Task.Run(() => UdpAsyncReadData(destroyCancellationToken), destroyCancellationToken); // Udp read Task start

            _sendRpcPacketByUdpTask = Task.Run(() => AsyncWriteByUdp(destroyCancellationToken), destroyCancellationToken); // rpc send by tcp Task start
            _sendRpcPacketByTcpTask = Task.Run(() => AsyncWriteByTcp(destroyCancellationToken), destroyCancellationToken); // rpc send by udp Task start

            _parsingRawPacketTask = Task.Run(() => AsyncParsingRawPacket(destroyCancellationToken), destroyCancellationToken);
        }

        private async Task<bool> AsyncExchangeUdpPort(CancellationToken ct)
        {
            try
            {
                var readPacketNetSize = new byte[4];

                // Read the size of the incoming data
                if (!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct))
                {
                    return false;
                }

                // Network to Host
                if (BitConverter.IsLittleEndian)
                    Array.Reverse(readPacketNetSize);

                var readPacketHostSize = BitConverter.ToUInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];

                // Read the rest of the data
                if (!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct))
                {
                    return false;
                }

                // Deserialize the data
                RpcPacket receiveUdpPort = RpcPacket.Parser.ParseFrom(readDataBuffer);

                // Network to Host
                var byteData = receiveUdpPort.Data.ToByteArray();
                if (byteData.Length != sizeof(ushort))
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"Invalid port data length: {byteData.Length}"));
                    return false;
                }

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(byteData);

                ushort udpPort = BitConverter.ToUInt16(byteData, 0);
                _dispatcher.Enqueue(() => Debug.Log($"Received Server Udp Port: {udpPort}"));

                // Udp Socket Init
                _udpClient = new UdpClient(new IPEndPoint(IPAddress.Any, 0));
                const int SIO_UDP_CONNRESET = -1744830452;
                byte[] inValue = new byte[] { 0 };
                byte[] outValue = new byte[] { 0 };
                _udpClient.Client.IOControl(SIO_UDP_CONNRESET, inValue, outValue);

                // Endpoint set
                _clientUdpEndPoint = (IPEndPoint)_udpClient.Client.LocalEndPoint!;
                _serverUdpEndPoint = new IPEndPoint(IPAddress.Parse(Host), udpPort);
                _dispatcher.Enqueue(() => Debug.Log($"Udp Client Created - {_serverUdpEndPoint.Address}:{_serverUdpEndPoint.Port}"));

                // Send the UDP port to the server
                var clientPort = (ushort)_clientUdpEndPoint.Port;
                _dispatcher.Enqueue(() => Debug.Log($"Send Port : {clientPort}"));

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
                _dispatcher.Enqueue(() => Debug.Log($"Exchanged Udp Port completed"));
            }
            catch (OperationCanceledException)
            {
                _dispatcher.Enqueue(() => Debug.Log($"Cancelled"));
                return false;
            }
            catch (Exception ex)
            {
                _dispatcher.Enqueue(() => Debug.LogError($"Exchange failed: {ex.Message}"));
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
                if (!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct))
                {
                    return false;
                }

                // deserialize receive packet
                RpcPacket userInfoData = RpcPacket.Parser.ParseFrom(readDataBuffer);
                if (userInfoData.Method != RpcMethod.UserInfo)
                    return false;

                RpcPacket sendUserInfoData = new()
                {
                    Uid = UserId.ToString(), // 랜덤 UID 사용
                    Method = RpcMethod.UserInfo,
                    Data = ByteString.CopyFrom(Encoding.UTF8.GetBytes(Name)) // Start에서 설정된 이름 사용
                };

                byte[] sendData = sendUserInfoData.ToByteArray(); // Serialize rpc packet to byte[]
                byte[] sendDataSize = BitConverter.GetBytes(sendData.Length);

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(sendDataSize);

                await _tcpStream.WriteAsync(sendDataSize, 0, sendDataSize.Length, ct);
                await _tcpStream.WriteAsync(sendData, 0, sendData.Length, ct);
                _dispatcher.Enqueue(() => Debug.Log($"Exchange UserInfo Complete"));
            }
            catch (OperationCanceledException)
            {
                _dispatcher.Enqueue(() => Debug.Log($"Cancelled"));
                return false;
            }
            catch (Exception ex)
            {
                _dispatcher.Enqueue(() => Debug.LogError($"Network Error {ex.Message}"));
                return false;
            }

            return true;
        }

        private async Task<bool> AsyncExchangeGroupInfo(CancellationToken ct)
        {
            try
            {
                var readPacketNetSize = new byte[4];
                if (!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct))
                {
                    return false;
                }

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(readPacketNetSize);

                var readPacketHostSize = BitConverter.ToInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];

                if (!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct))
                {
                    return false;
                }

                RpcPacket readPacket = RpcPacket.Parser.ParseFrom(readDataBuffer);
                if (readPacket.Method != RpcMethod.GroupInfo)
                    return false;

                RpcPacket sendPacket = new()
                {
                    Method = RpcMethod.GroupInfo,
                    Uid = UserId.ToString(), // 랜덤 UID 사용
                    Data = _currentGroupDto.ToByteString()
                };

                byte[] sendPacketData = sendPacket.ToByteArray(); // serialize rpc packet to byte[]
                byte[] sendPacketSize = BitConverter.GetBytes(sendPacketData.Length);

                if (BitConverter.IsLittleEndian)
                    Array.Reverse(sendPacketSize);

                await _tcpStream.WriteAsync(sendPacketSize, 0, sendPacketSize.Length, ct);
                await _tcpStream.WriteAsync(sendPacketData, 0, sendPacketData.Length, ct);
                _dispatcher.Enqueue(() => Debug.Log($"Exchange Complete"));

                return true;
            }
            catch (OperationCanceledException)
            {
                Debug.Log("Cancelled");
                return false;
            }
            catch (Exception ex)
            {
                _dispatcher.Enqueue(() => Debug.LogError($"Network Error: {ex.Message}"));
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
            await Awaitable.BackgroundThreadAsync();
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    if (!_sendPacketQueueForUdp.TryDequeue(out var data))
                    {
                        await Task.Yield();
                        continue;
                    }

                    data.Uid = UserId.ToString(); 
                    byte[] payload = data.ToByteArray();
                    var payloadSize = (short)payload.Length;
                    byte[] sizeBuffer = BitConverter.GetBytes(payloadSize);

                    if (BitConverter.IsLittleEndian)
                        Array.Reverse(sizeBuffer);

                    var packet = new byte[sizeBuffer.Length + payload.Length];
                    Buffer.BlockCopy(sizeBuffer, 0, packet, 0, sizeBuffer.Length);
                    Buffer.BlockCopy(payload, 0, packet, sizeBuffer.Length, payload.Length);

                    await _udpClient.SendAsync(packet, packet.Length, _serverUdpEndPoint);

                    Interlocked.Increment(ref _sentPacketCount); // sent packet count increment by atomic
                    _dispatcher.Enqueue(() => Debug.Log($"Sent Rpc Packet To Server {Utility.Util.ConvertTimestampToString(data.Timestamp)}" +
                        $" {data.Uid}: {data.Method}"));
                }
                catch (OperationCanceledException)
                {
                    _dispatcher.Enqueue(() => Debug.Log("Cancelled"));
                    return;
                }
                catch (ObjectDisposedException)
                {
                    _dispatcher.Enqueue(() => Debug.Log("stream closed"));
                    return;
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() =>
                    {
                        Debug.LogError($"send failed: {ex.Message}");
                        DisconnectFromServer();
                    });
                    return;
                }
            }
        }

        private void EnqueueRpcPacketForTcp(RpcPacket sendPacket)
        {
            _sendPacketQueueForTcp.Enqueue(sendPacket);
        }

        private async Task AsyncWriteByTcp(CancellationToken ct)
        {
            await Awaitable.BackgroundThreadAsync();
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    if (!_sendPacketQueueForTcp.TryDequeue(out var data))
                    {
                        await Task.Yield();
                        continue;
                    }

                    // Tcp Send
                    byte[] sendPacket = data.ToByteArray();
                    byte[] packetSize = BitConverter.GetBytes(sendPacket.Length);

                    if (BitConverter.IsLittleEndian)
                        Array.Reverse(packetSize);

                    await _tcpStream.WriteAsync(packetSize, 0, packetSize.Length, ct);
                    await _tcpStream.WriteAsync(sendPacket, 0, sendPacket.Length, ct);
                }
                catch (OperationCanceledException)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"cancelled"));
                    return;
                }
                catch (ObjectDisposedException)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"stream closed"));
                    return;
                }
                catch (Exception e)
                {
                    _dispatcher.Enqueue(() =>
                    {
                        Debug.LogError($"error tcp send: {e.Message}");
                        DisconnectFromServer();
                    });
                    return;
                }
            }
        }

        private async Task TcpAsyncReadData(CancellationToken ct)
        {
            await Awaitable.BackgroundThreadAsync();
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    Array.Clear(_receiveBufferHeader, 0, 4); // buffer clear
                    if (!await ReadTcpExactlyAsync(_tcpStream, _receiveBufferHeader, sizeof(uint), ct))
                    {
                        _dispatcher.Enqueue(() =>
                        {
                            Debug.Log($"failed to read packet size.");
                            DisconnectFromServer();
                        });
                        return;
                    }

                    // Convert the size from bytes to uint
                    if (BitConverter.IsLittleEndian)
                        Array.Reverse(_receiveBufferHeader);

                    var dataSize = BitConverter.ToInt32(_receiveBufferHeader, 0);
                    if (dataSize <= 0 || dataSize > MAX_PACKET_SIZE)
                    {
                        _dispatcher.Enqueue(() =>
                        {
                            Debug.LogError($"[TCP READ ERROR] Invalid Packet Size received: {dataSize}. Disconnecting.");
                            DisconnectFromServer();
                        });

                        return;
                    }

                    // Read the rest of the data
                    if (!await ReadTcpExactlyAsync(_tcpStream, _receiveBufferData, dataSize, ct))
                    {
                        _dispatcher.Enqueue(() =>
                        {
                            Debug.LogError($"failed to read packet data.");
                            DisconnectFromServer();
                        });
                        return;
                    }

                    var finalPacket = new byte[dataSize];
                    Buffer.BlockCopy(_receiveBufferData, 0, finalPacket, 0, dataSize);
                    EnqueueRawQueue(finalPacket);
                }
                catch (OperationCanceledException)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"cancelled"));
                    return;
                }
                catch (ObjectDisposedException)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"stream closed"));
                    return;
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() =>
                    {
                        Debug.LogError($"TCP read size error: {ex.Message}");
                        DisconnectFromServer();
                    });
                    return;
                }
            }
        }

        private async Task<bool> ReadTcpExactlyAsync(NetworkStream stream, byte[] buffer, int length, CancellationToken ct)
        {
            await Awaitable.BackgroundThreadAsync();
            int totalBytesRead = 0;
            while (totalBytesRead < length)
            {
                try
                {
                    int bytesRead = await stream.ReadAsync(buffer, totalBytesRead, length - totalBytesRead, ct); // 총 길이 - 현재 길이 만큼 더 받음
                    if (bytesRead == 0)
                    {
                        _dispatcher.Enqueue(() => Debug.LogWarning("(EOF) disconnected"));
                        return false;
                    }

                    totalBytesRead += bytesRead;
                }
                catch (OperationCanceledException)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"cancelled"));
                    return false;
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"error ReadTcpExactly: {ex.Message}"));
                    return false;
                }
            }

            return true;
        }

        private async Task UdpAsyncReadData(CancellationToken ct)
        {
            await Awaitable.BackgroundThreadAsync();
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    // Read the data from the stream
                    UdpReceiveResult result = await _udpClient.ReceiveAsync();
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
                    if (payloadSize == 0 || payloadSize > readData.Length - 2)
                    {
                        IncrementErrorCount();
                        continue;
                    }

                    // Extract the payload
                    var payload = new byte[payloadSize];
                    Buffer.BlockCopy(readData, 2, payload, 0, payloadSize);

                    EnqueueRawQueue(payload);
                    Interlocked.Increment(ref _receivedPacketCount); // Increment received packet count by atomic
                    _dispatcher.Enqueue(() => { Debug.Log($"read data in udp all passed"); });
                }
                catch (OperationCanceledException)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"UDP loop canceled"));
                    break;
                }
                catch (ObjectDisposedException)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"stream closed"));
                    break;
                }
                catch (Exception ex) // abnormal exit 
                {
                    _dispatcher.Enqueue(() =>
                    {
                        Debug.LogError($"UDP error: {ex.Message}");
                        DisconnectFromServer();
                    });
                    break;
                }
            }
        }

        /// <summary>
        /// called in background thread
        /// </summary>
        private void EnqueueRawQueue(byte[] data)
        {
            _rawQueue.Enqueue(data);
        }

        /// <summary>
        /// called in main thrad
        /// </summary>
        private byte[] DequeueRawQueue()
        {
            if (!_rawQueue.TryDequeue(out var data))
            {
                return null;
            }

            return data;
        }

        private void EnqueueReadyQueue(Tuple<RpcPacket, object> nextPacket)
        {
            _readyQueue.Enqueue(nextPacket);
        }

        private Tuple<RpcPacket, object> DequeueReadyQueue()
        {
            if (!_readyQueue.TryDequeue(out var nextPacket))
            {
                return null;
            }

            return nextPacket;
        }

        private async Task AsyncParsingRawPacket(CancellationToken ct)
        {
            await Awaitable.BackgroundThreadAsync();

            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    var nextPacket = DequeueRawQueue();
                    if (nextPacket is null)
                    {
                        await Task.Delay(5, ct);
                        continue;
                    }

                    var packetData = RpcPacket.Parser.ParseFrom(nextPacket);
                    if (packetData is null)
                    {
                        _dispatcher.Enqueue(() => { Debug.LogError($"parsing error occured in raw packet"); });
                        continue;
                    }


                    switch (packetData.Method)
                    {
                        case RpcMethod.Move:
                        case RpcMethod.MoveStart:
                        case RpcMethod.MoveStop:
                            MoveData moveData = MoveData.Parser.ParseFrom(packetData.Data);
                            if(moveData is null)
                            {
                                _dispatcher.Enqueue(() => { Debug.LogError($"parsing move data error occured in raw packet parser"); });
                                continue;
                            }

                            EnqueueReadyQueue(new Tuple<RpcPacket, object>(packetData, moveData));
                            break;

                        case RpcMethod.Atk:
                            AtkData atkData = AtkData.Parser.ParseFrom(packetData.Data);
                            if(atkData is null)
                            {
                                _dispatcher.Enqueue(() => { Debug.LogError($"parsing atk data error occured in raw packet parser"); });
                                continue;
                            }

                            EnqueueReadyQueue(new Tuple<RpcPacket, object>(packetData, atkData));
                            break;
                        case RpcMethod.Hit:
                            break;
                        case RpcMethod.Dead:
                            break;

                        case RpcMethod.ClientGameInfo:
                            GameData gameData = GameData.Parser.ParseFrom(packetData.Data);
                            if (gameData is null)
                            {
                                _dispatcher.Enqueue(() => { Debug.LogError($"parsing game client data error occured in raw packet parser"); });
                                continue;
                            }

                            EnqueueReadyQueue(new Tuple<RpcPacket, object>(packetData, gameData));
                            break;

                        case RpcMethod.Ping:
                        case RpcMethod.LastRtt:
                            EnqueueReadyQueue(new Tuple<RpcPacket, object>(packetData, null)); // by-pass
                            break;

                        default:
                            break;
                    }
                }
                catch (OperationCanceledException)
                {
                    return;
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() => { Debug.LogError($"invalid error occured: {ex.Message}"); });
                    return;
                }
            }
        }

        /// <summary>
        /// called in main thread
        /// </summary>
        private void ProcessRpc()
        {
            var data = DequeueReadyQueue();
            while (data is not null) // all packets in queue processed per frame
            {
                switch (data.Item1.Method)
                {
                    case RpcMethod.Ping:
                        var pongPacket = new RpcPacket
                        {
                            Method = RpcMethod.Pong,
                            Uid = UserId.ToString()
                        };

                        EnqueueRpcPacketForTcp(pongPacket);
                        break;

                    case RpcMethod.MoveStart:
                    case RpcMethod.MoveStop:
                    case RpcMethod.Move:
                        var moveData = (MoveData)data.Item2;
                        SyncManager.Instance.Enqueue(Guid.Parse(data.Item1.Uid), moveData);
                        break;

                    case RpcMethod.Atk:
                        // Deserialize AtkData
                        var atkData = (AtkData)data.Item2;
                        SyncManager.Instance.EnqueueAttackData(Guid.Parse(data.Item1.Uid), atkData);
                        break;

                    case RpcMethod.Hit:
                        break;

                    case RpcMethod.LastRtt:
                        LastRtt = int.Parse(data.Item1.Data.ToStringUtf8());
                        _rttList.Add(LastRtt);

                        RttAverage = (int)_rttList.Average(x => x);
                        LogManager.Instance.Log($"Get Rtt: {LastRtt}");
                        break;

                    case RpcMethod.PacketCount:
                    case RpcMethod.NetworkNone:
                        Debug.Assert(false, "Not Implemented");
                        break;

                    case RpcMethod.Pong:
                        Debug.Assert(false, "Not Client Method");
                        break;

                    case RpcMethod.ClientGameInfo:
                        var gameData = (GameData)data.Item2;
                        LogManager.Instance.Log($"{data.Item1.Uid} info : {gameData.Hp}, ({gameData.Position.X}, {gameData.Position.Y}, {gameData.Position.Z})");
                        break;

                    // Network Initialize
                    case RpcMethod.UdpPort:
                    case RpcMethod.UserInfo:
                    case RpcMethod.GroupInfo:
                    case RpcMethod.InGameNone:
                    default:
                        Debug.Assert(false, "Invalid Method");
                        break;
                }

                data = DequeueReadyQueue();
            }
        }
    }
}