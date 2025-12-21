using System;
﻿using System.Collections.Generic;
﻿using System.Linq;
﻿using System.Text;
﻿using System.Threading;
﻿using System.Threading.Tasks;
﻿using Google.Protobuf;
﻿using NetworkData;
﻿using UnityEngine;

namespace Network
{
    public class ManualConnector : BaseConnector
    {
        public static ManualConnector Instance { get; private set; }

        public override Guid UserId { get; protected set; }
        public string Name { get; private set; }
        public override List<UserSimpleDto> Users => _currentGroupDto?.PlayerList.ToList() ?? new List<UserSimpleDto>();

        protected override void Awake()
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
            UserId = Guid.NewGuid();
            Name = $"Player_{UserId.ToString().Substring(0, 6)}";
        }

        protected override void Update()
        {
            base.Update();
            if (!IsOnline)
            {
                if (Input.GetKeyDown(KeyCode.F1))
                {
                    var currentUser = new UserSimpleDto { Uid = UserId.ToString(), Username = Name };
                    var groupInfo = new GroupDto { GroupId = "a1b2c3d4-0000-0000-0000-1234567890ab", PlayerList = { currentUser } };
                    _ = TryConnectToServer(groupInfo, "127.0.0.1", 53200);
                }
            }
        }

        public async Task<bool> TryConnectToServer(GroupDto groupInfo, string ip, ushort port)
        {
            if (IsOnline) return false;
            if (IsTryingToConnect) return false;

            _currentGroupDto = groupInfo;
            Host = ip;
            Port = port;

            bool isConnected = await ConnectToServer(destroyCancellationToken);
            if (isConnected)
            {
                StartGameTask();
            }
            IsTryingToConnect = false;
            return isConnected;
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
                    Uid = UserId.ToString(),
                    Method = RpcMethod.UserInfo,
                    Data = ByteString.CopyFrom(Encoding.UTF8.GetBytes(Name))
                };
                byte[] sendData = sendUserInfoData.ToByteArray();
                byte[] sendDataSize = BitConverter.GetBytes(sendData.Length);
                if (BitConverter.IsLittleEndian) Array.Reverse(sendDataSize);

                await _tcpStream.WriteAsync(sendDataSize, 0, sendDataSize.Length, ct);
                await _tcpStream.WriteAsync(sendData, 0, sendData.Length, ct);

                _dispatcher.Enqueue(() => Debug.Log($"Exchange User Info Complete"));
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

                RpcPacket sendPacket = new()
                {
                    Method = RpcMethod.GroupInfo,
                    Uid = UserId.ToString(),
                    Data = _currentGroupDto.ToByteString()
                };

                byte[] sendPacketData = sendPacket.ToByteArray();
                byte[] sendPacketSize = BitConverter.GetBytes(sendPacketData.Length);
                if (BitConverter.IsLittleEndian) Array.Reverse(sendPacketSize);

                await _tcpStream.WriteAsync(sendPacketSize, 0, sendPacketSize.Length, ct);
                await _tcpStream.WriteAsync(sendPacketData, 0, sendPacketData.Length, ct);

                _dispatcher.Enqueue(() => Debug.Log($"Exchange Group Info Complete"));
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
                        var pongPacket = new RpcPacket { Method = RpcMethod.Pong, Uid = UserId.ToString() };
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
                    case RpcMethod.LastRtt:
                        int.TryParse(packet.Data.ToStringUtf8(), out var rtt);
                        // RTT handling is in base class
                        break;
                    case RpcMethod.ClientGameInfo:
                        // Log or handle
                        break;
                }
                data = DequeueReadyQueue();
            }
        }
    }
}
﻿