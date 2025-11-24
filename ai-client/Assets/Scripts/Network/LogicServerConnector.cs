using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf;
using NetworkData;
using UnityEngine;

namespace Network
{
    public class LogicServerConnector : BaseConnector
    {
        public static LogicServerConnector Instance { get; private set; }

        private AuthManager _authManager;
        private Guid _userId;
        public override Guid UserId
        {
            get => _userId;
            protected set => _userId = value;
        }
        public override List<UserSimpleDto> Users => _currentGroupDto?.PlayerList.ToList() ?? new List<UserSimpleDto>();

        void Awake()
        {
            base.Awake();
            if (Instance != null && Instance != this)
            {
                Destroy(gameObject);
                return;
            }
            Instance = this;
            DontDestroyOnLoad(gameObject);
        }

        protected override void Start()
        {
            base.Start();
            _authManager = AuthManager.Instance;
            UserId = _authManager.UserGuid;
        }

        public async Task<bool> TryConnectToServer(InRoomManager roomManager, GroupDto groupInfo, string ip, ushort port)
        {
            if (IsOnline)
            {
                _dispatcher.Enqueue(() => Debug.Log("already connected."));
                return false;
            }

            if (IsTryingToConnect)
            {
                _dispatcher.Enqueue(() => Debug.Log("already trying to connect."));
                return false;
            }

            _currentGroupDto = groupInfo;
            Host = ip;
            Port = port;

            bool isConnected = await ConnectToServer(destroyCancellationToken);
            if (!isConnected)
            {
                IsTryingToConnect = false;
                return false;
            }

            return true;
        }

        protected override async Task<bool> AsyncExchangeUserInfo(CancellationToken ct)
        {
            try
            {
                var readPacketNetSize = new byte[4];
                if (!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct)) return false;
                if (BitConverter.IsLittleEndian) Array.Reverse(readPacketNetSize);

                var readPacketHostSize = BitConverter.ToInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];
                if (!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct)) return false;

                RpcPacket userInfoData = RpcPacket.Parser.ParseFrom(readDataBuffer);
                if (userInfoData.Method != RpcMethod.UserInfo) return false;

                RpcPacket sendUserInfoData = new()
                {
                    Uid = _authManager.UserGuid.ToString(),
                    Method = RpcMethod.UserInfo,
                    Data = ByteString.CopyFrom(Encoding.UTF8.GetBytes(_authManager.Username))
                };

                byte[] sendData = sendUserInfoData.ToByteArray();
                byte[] sendDataSize = BitConverter.GetBytes(sendData.Length);
                if (BitConverter.IsLittleEndian) Array.Reverse(sendDataSize);

                await _tcpStream.WriteAsync(sendDataSize, 0, sendDataSize.Length, ct);
                await _tcpStream.WriteAsync(sendData, 0, sendData.Length, ct);
                return true;
            }
            catch (Exception ex)
            {
                _dispatcher.Enqueue(() => Debug.LogError($"Network Error {ex.Message}"));
                return false;
            }
        }

        protected override async Task<bool> AsyncExchangeGroupInfo(CancellationToken ct)
        {
            try
            {
                var readPacketNetSize = new byte[4];
                if (!await ReadTcpExactlyAsync(_tcpStream, readPacketNetSize, readPacketNetSize.Length, ct)) return false;
                if (BitConverter.IsLittleEndian) Array.Reverse(readPacketNetSize);

                var readPacketHostSize = BitConverter.ToInt32(readPacketNetSize, 0);
                var readDataBuffer = new byte[readPacketHostSize];
                if (!await ReadTcpExactlyAsync(_tcpStream, readDataBuffer, readDataBuffer.Length, ct)) return false;

                RpcPacket readPacket = RpcPacket.Parser.ParseFrom(readDataBuffer);
                if (readPacket.Method != RpcMethod.GroupInfo) return false;

                var sendDtoString = JsonFormatter.Default.Format(_currentGroupDto);
                RpcPacket sendPacket = new()
                {
                    Method = RpcMethod.GroupInfo,
                    Uid = _authManager.UserGuid.ToString(),
                    Data = ByteString.CopyFrom(Encoding.UTF8.GetBytes(sendDtoString))
                };

                byte[] sendPacketData = sendPacket.ToByteArray();
                byte[] sendPacketSize = BitConverter.GetBytes(sendPacketData.Length);
                if (BitConverter.IsLittleEndian) Array.Reverse(sendPacketSize);

                await _tcpStream.WriteAsync(sendPacketSize, 0, sendPacketSize.Length, ct);
                await _tcpStream.WriteAsync(sendPacketData, 0, sendPacketData.Length, ct);
                return true;
            }
            catch (Exception ex)
            {
                _dispatcher.Enqueue(() => Debug.LogError($"Network Error: {ex.Message}"));
                return false;
            }
        }

        protected override void ProcessRpc()
        {
            var data = DequeueReadyQueue();
            while (data != null)
            {
                var (packet, parsedData) = data;
                switch (packet.Method)
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
                        if (parsedData is MoveData moveData)
                        {
                            SyncManager.Instance.Enqueue(Guid.Parse(packet.Uid), packet.Method, moveData);
                        }
                        break;
                    case RpcMethod.Atk:
                        if (parsedData is AtkData atkData)
                        {
                            SyncManager.Instance.EnqueueAttackData(Guid.Parse(packet.Uid), atkData);
                        }
                        break;
                    case RpcMethod.ClientGameInfo:
                        if (parsedData is GameData gameData)
                        {
                            // Log
                        }
                        break;
                    default:
                        // Other Method's passed
                        break;
                }

                data = DequeueReadyQueue();
            }
        }
    }
}