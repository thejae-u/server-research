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
            Debug.Log($"send to server : {data.Uuid} {data.Method.ToString()} {positionData.X} {positionData.Y} {positionData.Z}");
        }
        catch (Exception ex)
        {
            Debug.LogError($"send to failed: {ex.Message}");
        }
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