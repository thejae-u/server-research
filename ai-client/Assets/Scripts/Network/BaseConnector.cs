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
    public abstract class BaseConnector : MonoBehaviour
    {
        public string Host { get; protected set; }
        public ushort Port { get; protected set; }
        public abstract Guid UserId { get; protected set; }
        public Action disconnectAction { get; set; }

        protected TcpClient _tcpClient;
        protected UdpClient _udpClient;
        protected IPEndPoint _serverUdpEndPoint;
        protected IPEndPoint _clientUdpEndPoint;
        protected NetworkStream _tcpStream;

        protected readonly ConcurrentQueue<byte[]> _rawQueue = new();
        protected readonly ConcurrentQueue<Tuple<RpcPacket, object>> _readyQueue = new();

        protected int _maxRetries = 5;
        protected const uint MAX_PACKET_SIZE = 65536; // 64KB

        private volatile int _sentPacketCount = 0;
        public int SentPacketCount => Interlocked.CompareExchange(ref _sentPacketCount, 0, 0);

        private volatile int _receivedPacketCount = 0;
        public int ReceivedPacketCount => Interlocked.CompareExchange(ref _receivedPacketCount, 0, 0);

        private volatile int _lastRtt = 0;
        public int LastRtt => Interlocked.CompareExchange(ref _lastRtt, 0, 0);

        private readonly List<int> _rttList = new();

        private volatile int _rttAverage = 0;
        public int RttAverage => Interlocked.CompareExchange(ref _rttAverage, 0, 0);

        private volatile int _errorCount = 0;
        public int ErrorCount => Interlocked.CompareExchange(ref _errorCount, 0, 0);
        public void IncrementErrorCount() { Interlocked.Increment(ref _errorCount); }

        public abstract List<UserSimpleDto> Users { get; }
        protected GroupDto _currentGroupDto = null;

        private Task _tcpReadTask;
        private Task _udpReadTask;
        private Task _parsingRawPacketTask;
        private Task _sendRpcPacketByUdpTask;
        private Task _sendRpcPacketByTcpTask;

        protected readonly ConcurrentQueue<RpcPacket> _sendPacketQueueForUdp = new();
        protected readonly ConcurrentQueue<RpcPacket> _sendPacketQueueForTcp = new();

        public float ErrorRate
        {
            get
            {
                if (ErrorCount == 0) return 0.0f;
                return ErrorCount / (float)ReceivedPacketCount * 100.0f;
            }
        }

        private volatile int _isOnline;
        public bool IsOnline => Interlocked.CompareExchange(ref _isOnline, 0, 0) == 1;

        protected bool IsTryingToConnect { get; set; }
        protected MainThreadDispatcher _dispatcher;

        protected virtual void Awake()
        {
            _dispatcher = MainThreadDispatcher.Instance;
        }

        protected virtual void Start()
        {
            IsTryingToConnect = false;
        }

        protected virtual void OnDestroy()
        {
            DisconnectFromServer();
        }

        protected virtual void OnApplicationQuit()
        {
            DisconnectFromServer();
        }

        protected virtual void OnEnable()
        {
            disconnectAction += DisconnectFromServer;
        }

        protected virtual void OnDisable()
        {
            disconnectAction -= DisconnectFromServer;
        }

        protected virtual void Update()
        {
            if (IsOnline)
            {
                ProcessRpc();
            }
        }

        protected async Task<bool> ConnectToServer(CancellationToken ct)
        {
            if (IsOnline) return false;

            IsTryingToConnect = true;
            _tcpClient = new TcpClient();

            _dispatcher.Enqueue(() => Debug.Log("Connecting to server Start"));
            for (int i = 0; i <= _maxRetries; ++i)
            {
                try
                {
                    await _tcpClient.ConnectAsync(Host, Port);
                    break;
                }
                catch (SocketException ex)
                {
                    _dispatcher.Enqueue(() => Debug.Log($"Connection failed: {ex.Message}"));
                    if (i < _maxRetries)
                    {
                        _dispatcher.Enqueue(() => Debug.Log($"Retrying... {_maxRetries - i}"));
                    }
                    else
                    {
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

            if (!await AsyncExchangeUdpPort(ct))
            {
                DisconnectFromServer();
                return false;
            }

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

            _dispatcher.Enqueue(() => Debug.Log("All Handshake Done. Connection Complete"));
            return true;
        }

        public void StartGameTask()
        {
            if (IsOnline) return;

            Interlocked.Exchange(ref _isOnline, 1);
            IsTryingToConnect = false;

            _sentPacketCount = 0;
            _receivedPacketCount = 0;

            var ct = destroyCancellationToken;
            _tcpReadTask = Task.Run(() => TcpAsyncReadData(ct), ct);
            _udpReadTask = Task.Run(() => UdpAsyncReadData(ct), ct);
            _sendRpcPacketByUdpTask = Task.Run(() => AsyncWriteByUdp(ct), ct);
            _sendRpcPacketByTcpTask = Task.Run(() => AsyncWriteByTcp(ct), ct);
            _parsingRawPacketTask = Task.Run(() => AsyncParsingRawPacket(ct), ct);
        }

        protected void DisconnectFromServer()
        {
            if (!IsOnline) return;

            Interlocked.Exchange(ref _isOnline, 0);
            _currentGroupDto = null;

            _tcpStream?.Close();
            _tcpClient?.Close();

            disconnectAction?.Invoke();

            Debug.Log("disconnected.");
        }

        public void EnqueueRpcPacketForUdp(RpcPacket sendPacket)
        {
            _sendPacketQueueForUdp.Enqueue(sendPacket);
        }

        public void EnqueueRpcPacketForTcp(RpcPacket sendPacket)
        {
            _sendPacketQueueForTcp.Enqueue(sendPacket);
        }

        protected abstract Task<bool> AsyncExchangeUserInfo(CancellationToken ct);
        protected abstract Task<bool> AsyncExchangeGroupInfo(CancellationToken ct);
        protected abstract void ProcessRpc();

        private async Task<bool> AsyncExchangeUdpPort(CancellationToken ct)
        {
            // This implementation is common
            try
            {
                var readPacketNetSize = new byte[4];
                if (!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct)) return false;

                if (BitConverter.IsLittleEndian) Array.Reverse(readPacketNetSize);
                var readPacketHostSize = BitConverter.ToUInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];

                if (!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct)) return false;

                RpcPacket receiveUdpPort = RpcPacket.Parser.ParseFrom(readDataBuffer);
                var byteData = receiveUdpPort.Data.ToByteArray();
                if (byteData.Length != sizeof(ushort))
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"Invalid port data length: {byteData.Length}"));
                    return false;
                }

                if (BitConverter.IsLittleEndian) Array.Reverse(byteData);
                ushort udpPort = BitConverter.ToUInt16(byteData, 0);
                _dispatcher.Enqueue(() => Debug.Log($"Received Server Udp Port: {udpPort}"));

                _udpClient = new UdpClient(new IPEndPoint(IPAddress.Any, 0));
                const int SIO_UDP_CONNRESET = -1744830452;
                _udpClient.Client.IOControl(SIO_UDP_CONNRESET, new byte[] { 0 }, new byte[] { 0 });

                _clientUdpEndPoint = (IPEndPoint)_udpClient.Client.LocalEndPoint;
                _serverUdpEndPoint = new IPEndPoint(IPAddress.Parse(Host), udpPort);
                _dispatcher.Enqueue(() => Debug.Log($"Udp Client Created - {_serverUdpEndPoint.Address}:{_serverUdpEndPoint.Port}"));

                var clientPort = (ushort)_clientUdpEndPoint.Port;
                var netClientPort = BitConverter.GetBytes(clientPort);
                if (BitConverter.IsLittleEndian) Array.Reverse(netClientPort);

                var sendUdpPortPacket = new RpcPacket
                {
                    Method = RpcMethod.UdpPort,
                    Data = ByteString.CopyFrom(netClientPort),
                };

                byte[] sendUdpPortPacketData = sendUdpPortPacket.ToByteArray();
                byte[] udpPortSize = BitConverter.GetBytes(sendUdpPortPacketData.Length);
                if (BitConverter.IsLittleEndian) Array.Reverse(udpPortSize);

                await _tcpStream.WriteAsync(udpPortSize, 0, udpPortSize.Length, ct);
                await _tcpStream.WriteAsync(sendUdpPortPacketData, 0, sendUdpPortPacketData.Length, ct);
                _dispatcher.Enqueue(() => Debug.Log($"Exchanged Udp Port completed"));
            }
            catch (Exception ex)
            {
                _dispatcher.Enqueue(() => Debug.LogError($"Exchange failed: {ex.Message}"));
                return false;
            }
            return true;
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
                    if (BitConverter.IsLittleEndian) Array.Reverse(sizeBuffer);

                    var packet = new byte[sizeBuffer.Length + payload.Length];
                    Buffer.BlockCopy(sizeBuffer, 0, packet, 0, sizeBuffer.Length);
                    Buffer.BlockCopy(payload, 0, packet, sizeBuffer.Length, payload.Length);

                    await _udpClient.SendAsync(packet, packet.Length, _serverUdpEndPoint);
                    Interlocked.Increment(ref _sentPacketCount);
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"send failed: {ex.Message}"));
                    DisconnectFromServer();
                    return;
                }
            }
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

                    byte[] sendPacket = data.ToByteArray();
                    byte[] packetSize = BitConverter.GetBytes(sendPacket.Length);
                    if (BitConverter.IsLittleEndian) Array.Reverse(packetSize);

                    await _tcpStream.WriteAsync(packetSize, 0, packetSize.Length, ct);
                    await _tcpStream.WriteAsync(sendPacket, 0, sendPacket.Length, ct);
                }
                catch (Exception e)
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"error tcp send: {e.Message}"));
                    DisconnectFromServer();
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
                    var sizeBuffer = new byte[sizeof(uint)];
                    if (!await ReadTcpExactlyAsync(_tcpStream, sizeBuffer, sizeBuffer.Length, ct))
                    {
                        _dispatcher.Enqueue(DisconnectFromServer);
                        return;
                    }
                    if (BitConverter.IsLittleEndian) Array.Reverse(sizeBuffer);
                    uint netSize = BitConverter.ToUInt32(sizeBuffer, 0);

                    if (netSize == 0 || netSize > MAX_PACKET_SIZE)
                    {
                        _dispatcher.Enqueue(DisconnectFromServer);
                        return;
                    }
                    var dataBuffer = new byte[netSize];
                    if (!await ReadTcpExactlyAsync(_tcpStream, dataBuffer, dataBuffer.Length, ct))
                    {
                        _dispatcher.Enqueue(DisconnectFromServer);
                        return;
                    }
                    _rawQueue.Enqueue(dataBuffer);
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"TCP read error: {ex.Message}"));
                    DisconnectFromServer();
                    return;
                }
            }
        }

        protected async Task<bool> ReadTcpExactlyAsync(NetworkStream stream, byte[] buffer, int length, CancellationToken ct)
        {
            await Awaitable.BackgroundThreadAsync();
            int totalBytesRead = 0;
            while (totalBytesRead < length)
            {
                try
                {
                    int bytesRead = await stream.ReadAsync(buffer, totalBytesRead, length - totalBytesRead, ct);
                    if (bytesRead == 0) return false;
                    totalBytesRead += bytesRead;
                }
                catch
                {
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
                    UdpReceiveResult result = await _udpClient.ReceiveAsync();
                    byte[] readData = result.Buffer;

                    if (readData.Length < sizeof(ushort))
                    {
                        IncrementErrorCount();
                        continue;
                    }

                    short netOrderSize = BitConverter.ToInt16(readData, 0);
                    ushort payloadSize = (ushort)IPAddress.NetworkToHostOrder(netOrderSize);

                    if (payloadSize == 0 || payloadSize > readData.Length - 2)
                    {
                        IncrementErrorCount();
                        continue;
                    }

                    var payload = new byte[payloadSize];
                    Buffer.BlockCopy(readData, 2, payload, 0, payloadSize);

                    _rawQueue.Enqueue(payload);
                    Interlocked.Increment(ref _receivedPacketCount);
                }
                catch (Exception ex)
                {
                    _dispatcher.Enqueue(() => Debug.LogError($"UDP error: {ex.Message}"));
                    DisconnectFromServer();
                    break;
                }
            }
        }

        protected void EnqueueReadyQueue(Tuple<RpcPacket, object> nextPacket)
        {
            _readyQueue.Enqueue(nextPacket);
        }

        protected Tuple<RpcPacket, object> DequeueReadyQueue()
        {
            return _readyQueue.TryDequeue(out var nextPacket) ? nextPacket : null;
        }

        private async Task AsyncParsingRawPacket(CancellationToken ct)
        {
            await Awaitable.BackgroundThreadAsync();
            while (!ct.IsCancellationRequested && IsOnline)
            {
                try
                {
                    if (!_rawQueue.TryDequeue(out var nextPacket))
                    {
                        await Task.Delay(5, ct);
                        continue;
                    }

                    var packetData = RpcPacket.Parser.ParseFrom(nextPacket);
                    object parsedDataObject = null;

                    switch (packetData.Method)
                    {
                        case RpcMethod.Move:
                        case RpcMethod.MoveStart:
                        case RpcMethod.MoveStop:
                            parsedDataObject = MoveData.Parser.ParseFrom(packetData.Data);
                            break;
                        case RpcMethod.Atk:
                            parsedDataObject = AtkData.Parser.ParseFrom(packetData.Data);
                            break;
                        case RpcMethod.Hit:
                            parsedDataObject = HitData.Parser.ParseFrom(packetData.Data);
                            break;
                        case RpcMethod.ClientGameInfo:
                            parsedDataObject = GameData.Parser.ParseFrom(packetData.Data);
                            break;
                    }

                    if (packetData.Data.Length > 0 && parsedDataObject is null)
                    {
                        // parsing error
                        continue;
                    }
                    EnqueueReadyQueue(new Tuple<RpcPacket, object>(packetData, parsedDataObject));
                }
                catch (Exception ex)
                {
                    Debug.LogError($"Parsing error: {ex.Message}");
                }
            }
        }
    }
}
