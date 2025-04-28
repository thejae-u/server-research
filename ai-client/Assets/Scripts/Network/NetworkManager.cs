using System;
using UnityEngine;
using System.Net.Sockets;
using Cysharp.Threading.Tasks;
using NetworkData;
using Google.Protobuf;

public class NetworkManager : Singleton<NetworkManager> 
{
    private TcpClient _client;
    private NetworkStream _stream;
    private int _maxRetries = 3;

    private uint _netSize;

    public bool IsOnline { get; private set; }

    private void Start()
    {
        // Connect to the server when the game starts
        ConnectToServer().Forget();
    }


    private void OnDestroy()
    {
        DisconnectFromServer();
    }

    private async UniTask ConnectToServer()
    {
        if (IsOnline)
        {
            return;
        }

        try
        {
            _client = new TcpClient();
            await _client.ConnectAsync("127.0.0.1", 53200);
            _stream = _client.GetStream();
            IsOnline = true;

            Debug.Log("Connected to server.");
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
        }
        
        // Start receiving data from the server
        ReceiveAsyncFromServer().Forget();
    }

    public async UniTask SendAsync(RpcPacket data)
    {
        if (!IsOnline)
        {
            return;
        }

        try
        {
            // Serialize the data to a string 
            byte[] sendData = ProtoSerializer.SerializeNetworkData(data);

            // send size first
            uint dataSize = (uint)sendData.Length;
            byte[] sizeBytes = BitConverter.GetBytes(dataSize);
            
            // Convert to little-endian if necessary
            if (BitConverter.IsLittleEndian)
            {
                Array.Reverse(sizeBytes);
            }

            // Send size
            await _stream.WriteAsync(sizeBytes, 0, sizeBytes.Length);

            // Send data 
            await _stream.WriteAsync(sendData, 0, sendData.Length);
            
            PositionData positionData = PositionData.Parser.ParseFrom(data.Data);
            Debug.Log($"send to server : {data.Uuid} {data.Method.ToString()}");
        }
        catch (Exception ex)
        {
            Debug.LogError($"send to failed: {ex.Message}");
        }
    }

    private async UniTask ReceiveAsyncFromServer()
    {
        while (IsOnline)
        {
            try
            {
                var sizeBuffer = new byte[sizeof(uint)];
                
                // Read the size of the incoming data
                int bytesRead = await _stream.ReadAsync(sizeBuffer, 0, sizeBuffer.Length);
                if (bytesRead == 0) // EOF connection closed
                {
                    DisconnectFromServer();
                    break;
                }
                
                // Convert the size from bytes to uint
                if (BitConverter.IsLittleEndian)
                {
                    Array.Reverse(sizeBuffer);
                }
                
                _netSize = BitConverter.ToUInt32(sizeBuffer, 0);
                var dataBuffer = new byte[_netSize];
                
                // Read the rest of the data
                ReadData(dataBuffer);
            }
            catch (Exception)
            {
                DisconnectFromServer();
                break;
            }

            _netSize = 0;
            await UniTask.Yield();
        }
    }

    private void ReadData(byte[] dataBuffer)
    {
        try
        {
            if (_netSize == 0)
                return;

            // Read the data from the stream
            int bytesRead = _stream.Read(dataBuffer, 0, dataBuffer.Length);
            if (bytesRead == 0) // EOF connection closed
            {
                DisconnectFromServer();
                return;
            }

            Debug.Log($"Received Rpc Packet From Server");
            // Deserialize the data
            RpcPacket data = ProtoSerializer.DeserializeNetworkData(dataBuffer);
            ProcessReceivedDataAsync(data).Forget(); // Process the data asynchronously
            
        }
        catch (Exception ex)
        {
            Debug.LogError($"receive from failed: {ex.Message}");
        }
    }
    
    private async UniTask ProcessReceivedDataAsync(RpcPacket data)
    {
        switch (data.Method)
        {
            case RpcMethod.Move:
                // Deserialize the data
                PositionData positionData = PositionData.Parser.ParseFrom(data.Data);
                
                var startPosition = new Vector3(positionData.X1, positionData.Y1, positionData.Z1);
                var targetPosition = new Vector3(positionData.X2, positionData.Y2, positionData.Z2);
                
                // Call SyncManager to sync the object position
                SyncManager.Instance.SyncObjectPosition(data.Uuid, startPosition, targetPosition, positionData.Duration); 
                break;
            
            case RpcMethod.Attack:
            case RpcMethod.DropItem:
            case RpcMethod.UseItem:
            case RpcMethod.UseSkill:
            case RpcMethod.RemoteMoveCall:
            case RpcMethod.RemoteAttackCall:
                Debug.Assert(false, "Not Implemented");
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
                Debug.Assert(false, "No Method");
                break;
            
            default:
                Debug.Assert(false, "Invalid Method");
                break;
        }
        
        await UniTask.Yield();
    }

    private void DisconnectFromServer()
    {
        if (!IsOnline)
        {
            return;
        }

        _stream?.Close();
        _client?.Close();
        IsOnline = false;

        Debug.Log("disconnected.");
    }
}