using System.Diagnostics;
using System.Linq.Expressions;
using System.Net;
using System.Net.Sockets;
using System.Text;
using Google;
using Google.Protobuf;

using NetworkData;

namespace multi_client_test;

public class Client
{
    private Guid _uuid;
    private string _ip;
    private ushort _port;
    private TcpClient _client;

    private readonly object _lock = new();
    private bool _isOnline;

    private Task? _sendLoopTask;
    private Task? _receiveLoopTask;
    private CancellationTokenSource _cts = new();

    public Guid Uuid => _uuid;

    public bool IsOnline
    {
        get
        {
            lock (_lock)
            {
                return _isOnline;
            }
        }

        set
        {
            lock (_lock)
            {
                _isOnline = value;
            }
        }
    }

    public Client(string ip, ushort port)
    {
        _ip = ip;
        _port = port;
        _uuid = Guid.Empty;
        _client = new TcpClient();
        IsOnline = true;
    }

    public async Task AsyncDisconnect()
    {
        Console.WriteLine("Disconnecting...");
        IsOnline = false;
        await _cts.CancelAsync();

        if (_client.Connected)
            _client.Close();

        if (_sendLoopTask != null && _receiveLoopTask != null)
        {
            _sendLoopTask.Dispose();
            _receiveLoopTask.Dispose();
            _sendLoopTask = null;
            _receiveLoopTask = null;
        }
    }

    private void Disconnect()
    {
        AsyncDisconnect().GetAwaiter().GetResult();
    }

    public async Task AsyncConnect()
    {
        // 비동기 연결을 위한 코드
        try
        {
            await _client.ConnectAsync(_ip, _port);
            Console.WriteLine($"서버에 연결되었습니다. IP: {_ip}, Port: {_port}");

            // 연결 성공 시 RTT 측정
            await AsyncReceive();

            // RTT 측정 성공 시 서버로부터 UUID 수신
            await AsyncReceive();
            IsOnline = true;

            // 비동기 데이터 전송 및 수신 루프 시작
            _sendLoopTask = Task.Run(AsyncSendLoop);
            _receiveLoopTask = Task.Run(AsyncReceiveLoop);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"서버 연결 실패: {ex.Message}");
            Disconnect();
        }
    }

    private async Task AsyncSendRttPong()
    {
        try
        {
            NetworkStream stream = _client.GetStream();

            var sendPacket = new RpcPacket
            {
                Method = RpcMethod.Pong,
            };

            byte[]? data = sendPacket.ToByteArray();
            byte[] size = BitConverter.GetBytes(data.Length);

            if (data == null)
                throw new Exception("data is null");

            if (BitConverter.IsLittleEndian)
                Array.Reverse(size); // Little Endian으로 변환

            await stream.WriteAsync(size);
            await stream.WriteAsync(data);
        }
        catch (Exception e)
        {
            Console.WriteLine($"RTT Pong 전송 실패: {e.Message}");
        }
    }


    private async Task AsyncSend()
    {
        RpcPacket sendPacket = CreateRandomPacket();

        try
        {
            // 비동기 데이터 전송을 위한 코드
            NetworkStream stream = _client.GetStream();
            byte[]? data = sendPacket.ToByteArray();
            byte[] size = BitConverter.GetBytes(data.Length);
            if (BitConverter.IsLittleEndian)
                Array.Reverse(size); // Little Endian으로 변환

            // 데이터 크기를 먼저 전송
            await stream.WriteAsync(size);
            await stream.WriteAsync(data);
            //Console.WriteLine($"{Uuid} 메시지를 전송했습니다.");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"{Uuid} 메시지 전송 실패: {ex.Message}");
            Disconnect();
        }

        await Task.Yield();
    }

    private async Task AsyncReceive()
    {
        try
        {
            // 비동기 데이터 수신을 위한 코드
            NetworkStream stream = _client.GetStream();
            var bufSize = new byte[4];
            int bytesRead = await stream.ReadAsync(bufSize, 0, bufSize.Length);

            if (bytesRead == 0)
            {
                Console.WriteLine($"{Uuid} 서버와의 연결이 끊어졌습니다.");
                Disconnect();
                return;
            }

            // Little Endian으로 변환
            if (BitConverter.IsLittleEndian)
                Array.Reverse(bufSize);

            var size = BitConverter.ToUInt32(bufSize, 0);
            var buf = new byte[size];

            bytesRead = await stream.ReadAsync(buf, 0, buf.Length);
            if (bytesRead == 0)
            {
                Console.WriteLine($"{Uuid} 서버와의 연결이 끊어졌습니다.");
                Disconnect();
                return;
            }

            RpcPacket receivePacket = RpcPacket.Parser.ParseFrom(buf);
            await AsyncProcessPacket(receivePacket);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"{Uuid} 메시지 수신 실패: {ex.Message}");
            Disconnect();
        }

        await Task.Yield();
    }

    private async Task AsyncProcessPacket(RpcPacket packet)
    {
        switch (packet.Method)
        {
            case RpcMethod.Move:
                // Move 처리
                PositionData positionData = PositionData.Parser.ParseFrom(packet.Data);
                Console.WriteLine($"{Uuid} - Move: {positionData.X1}, {positionData.Y1}, {positionData.Z1} -> {positionData.X2}, {positionData.Y2}, {positionData.Z2}");
                break;
            case RpcMethod.None:
                // GUID 처리
                byte[] uuidBytes = packet.Uuid.ToByteArray();
                _uuid = new Guid(uuidBytes);
                Console.WriteLine($"GUID 수신 : {_uuid}");
                break;
            case RpcMethod.Ping:
                await AsyncSendRttPong();
                break;
            default:
                break;
        }
        
        await Task.Yield();
    }

    private async Task AsyncSendLoop()
    {
        try
        {
            while (IsOnline && !_cts.Token.IsCancellationRequested)
            {
                await AsyncSend();
                await Task.Delay(Random.Shared.Next(30, 100)); // 1초에서 5초 사이의 랜덤 대기 시간
            }
        }
        catch (OperationCanceledException)
        {

        }
        catch (Exception ex)
        {
            Console.WriteLine($"{Uuid} 전송 루프 실패: {ex.Message}");
            Disconnect();
        }
    }
    
    private async Task AsyncReceiveLoop()
    {
        try
        {
            while (IsOnline && !_cts.Token.IsCancellationRequested)
            {
                await AsyncReceive();
            }
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception ex)
        {
            Console.WriteLine($"{Uuid} 수신 루프 실패: {ex.Message}");
            Disconnect();
        }
    }

    private RpcPacket CreateRandomPacket()
    {
        var positionData = new PositionData
        {
            X1 = Random.Shared.Next(-10, 10),
            Y1 = 1,
            Z1 = Random.Shared.Next(-10, 10),
            X2 = Random.Shared.Next(-10, 10),
            Y2 = 1,
            Z2 = Random.Shared.Next(-10, 10),
        }; 
        
        // UUID To ByteString
        var uuidBytes = _uuid.ToByteArray();
        ByteString uuidByteString = ByteString.CopyFrom(uuidBytes);
        
        var packet = new RpcPacket
        {
            Uuid = uuidByteString,
            Method = RpcMethod.Move,
            Data = positionData.ToByteString(),
            Timestamp = Google.Protobuf.WellKnownTypes.Timestamp.FromDateTime(DateTime.UtcNow)
        };
        
        return packet;
    }
}