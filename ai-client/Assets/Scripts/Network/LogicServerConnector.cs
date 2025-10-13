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
using NetworkData;
using UnityEngine;
using Utility;

namespace Network
{
    public class LogicServerConnector : Singleton<LogicServerConnector>
    {
        public string host { get; private set; }
        public ushort port { get; private set; }

        public Guid ConnectedUuid { get; private set; }

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

        private Task _tcpReadTask;
        private Task _udpReadTask;

        public bool IsOnline { get; private set; }
        public bool IsSendPacketOn { get; private set; }
        private bool IsTryingToConnect { get; set; }

        private void Start()
        {
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

        public void TryConnectToServer()
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

            ConnectToServer();
        }

        private async void ConnectToServer()
        {
            if (IsOnline)
                return;

            IsTryingToConnect = true;
            _tcpClient = new TcpClient();

            Debug.Log("Connecting to server Start");

            for (int i = 0; i <= _maxRetries; ++i)
            {
                try
                {
                    await _tcpClient.ConnectAsync(host, port);
                    break;
                }
                catch (SocketException ex)
                {
                    Debug.Log($"서버 연결 실패: {ex.Message}");

                    if (i <= _maxRetries)
                    {
                        Debug.Log($"재시도...{_maxRetries - i}");
                    }
                    else
                    {
                        return;
                    }
                }
                catch (Exception ex)
                {
                    Debug.LogError($"Unknown Error: {ex.Message}");
                    return;
                }
            }

            _tcpStream = _tcpClient.GetStream();

            // Get Udp Port
            if (!await AsyncExchangeUdpPort(_globalCancellationToken))
            {
                Debug.LogError("Connection failed");
                DisconnectFromServer();
                return;
            }

            // TODO : Send Guid

            _sentPacketCount = 0;
            _receivedPacketCount = 0;

            IsOnline = true;

            _tcpReadTask = TcpAsyncReadData(_globalCancellationToken);
            _udpReadTask = UdpAsyncReadData(_globalCancellationToken);

            IsTryingToConnect = false;
        }

        private async Task<bool> AsyncExchangeUdpPort(CancellationToken ct)
        {
            var udpPortPacketSize = new byte[4];
            int bytesRead = 0;

            try
            {
                // Read the size of the incoming data
                bytesRead = await _tcpStream.ReadAsync(udpPortPacketSize, 0, udpPortPacketSize.Length, ct);
            }
            catch (OperationCanceledException)
            {
                return false;
            }
            catch (Exception ex)
            {
                Debug.LogError($"Exchange port read size failed: {ex.Message}");
                return false;
            }

            if (bytesRead == 0) // EOF connection closed
            {
                DisconnectFromServer();
                return false;
            }

            // Convert the size from bytes to uint
            if (BitConverter.IsLittleEndian)
                Array.Reverse(udpPortPacketSize);

            var dataSize = BitConverter.ToUInt32(udpPortPacketSize, 0);
            var dataBuffer = new byte[dataSize];

            try
            {
                // Read the rest of the data
                bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, ct);
            }
            catch (OperationCanceledException)
            {
                return false;
            }
            catch (Exception ex)
            {
                Debug.LogError($"Exchange port read data failed: {ex.Message}");
                return false;
            }

            // EOF connection closed
            if (bytesRead == 0)
            {
                DisconnectFromServer();
                return false;
            }

            // Deserialize the data
            RpcPacket udpPortData = ProtoSerializer.DeserializeNetworkData(dataBuffer);
            ushort udpPort = Util.ConvertByteStringToUShort(udpPortData.Data);

            _udpClient = new UdpClient(new IPEndPoint(IPAddress.Any, 0));
            _clientUdpEndPoint = (IPEndPoint)_udpClient.Client.LocalEndPoint!;
            _serverUdpEndPoint = new IPEndPoint(IPAddress.Parse(host), udpPort);
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

            try
            {
                // Send the UDP port packet
                await _tcpStream.WriteAsync(udpPortSize, 0, udpPortSize.Length, ct);
                await _tcpStream.WriteAsync(sendUdpPortPacketData, 0, sendUdpPortPacketData.Length, ct);
                Debug.Log($"Exchanged Udp Port completed");
            }
            catch (OperationCanceledException)
            {
                return false;
            }
            catch (Exception ex)
            {
                Debug.LogError($"Exchange port send error: {ex.Message}");
                return false;
            }

            return true;
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

        public async Task AsyncWriteRpcPacket(RpcPacket data)
        {
            if (!IsOnline)
                return;

            try
            {
                data.Uuid = ProtoSerializer.SerializeUuid(ConnectedUuid);

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
                          $" {ProtoSerializer.ConvertUuidToGuid(data.Uuid).ToString()} : {data.Method}");
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

            // Tcp Send
            byte[] sendPacket = ProtoSerializer.SerializeNetworkData(data);
            byte[] packetSize = BitConverter.GetBytes(sendPacket.Length);

            if (BitConverter.IsLittleEndian)
                Array.Reverse(packetSize);

            try
            {
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
            }
        }

        private async Task TcpAsyncReadData(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested || IsOnline)
            {
                var sizeBuffer = new byte[sizeof(uint)]; // 4 bytes
                int bytesRead = 0;
                bool isSuccess = false;

                try
                {
                    // Read Size first for data receive
                    bytesRead = await _tcpStream.ReadAsync(sizeBuffer, 0, sizeBuffer.Length, ct);
                    isSuccess = true;
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Debug.LogError($"TCP read size error: {ex.Message}");
                    isSuccess = false;
                }

                if (!isSuccess)
                {
                    continue;
                }

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
                isSuccess = false;

                try
                {
                    // Read the rest of the data
                    bytesRead = await _tcpStream.ReadAsync(dataBuffer, 0, dataBuffer.Length, ct);
                    isSuccess = true;
                }
                catch (OperationCanceledException)
                {
                    Debug.Log("Canceled Tcp Read");
                    break;
                }
                catch (Exception ex)
                {
                    Debug.LogError($"Tcp read data error: {ex.Message}");
                    isSuccess = false;
                }

                // EOF connection closed
                if (bytesRead == 0)
                {
                    DisconnectFromServer();
                    break;
                }

                EnqueueProcess(dataBuffer);
            }
        }

        private async Task UdpAsyncReadData(CancellationToken ct)
        {
            while (!ct.IsCancellationRequested && IsOnline)
            {
                // Read the data from the stream
                UdpReceiveResult result;
                bool isSuccess = false;

                try
                {
                    result = await _udpClient.ReceiveAsync();
                    isSuccess = true;
                }
                catch (OperationCanceledException)
                {
                    Debug.Log($"UDP loop canceled");
                    break;
                }
                catch (Exception ex)
                {
                    Debug.LogError($"UDP error: {ex.Message}");
                    isSuccess = false;
                }

                // Error 시 버림
                if (!isSuccess)
                {
                    lock (_errorCountLock)
                    {
                        ErrorCount++;
                    }
                }

                byte[] readData = result.Buffer;

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
                case RpcMethod.None:
                    SyncManager.Instance.SyncObjectNone(ProtoSerializer.ConvertUuidToGuid(data.Uuid));
                    break;

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

                    var task = AsyncWriteByTcp(pongPacket, _globalCancellationToken);
                    break;

                case RpcMethod.MoveStart:
                case RpcMethod.MoveStop:
                case RpcMethod.Move:
                    // Deserialize the data
                    MoveData moveData = MoveData.Parser.ParseFrom(data.Data);

                    // Call SyncManager to sync the object position
                    SyncManager.Instance.SyncObjectPosition(ProtoSerializer.ConvertUuidToGuid(data.Uuid), moveData);
                    break;

                case RpcMethod.LastRtt:
                    byte[] rttData = data.Data.ToByteArray();
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

                case RpcMethod.Login:
                case RpcMethod.Register:
                case RpcMethod.Retrieve:
                case RpcMethod.Access:
                case RpcMethod.Reject:
                case RpcMethod.Logout:
                case RpcMethod.UdpPort:
                    Debug.Assert(false, "Not Sync Method");
                    break;

                case RpcMethod.InGameNone:
                default:
                    Debug.Assert(false, "Invalid Method");
                    break;
            }
        }
    }
}