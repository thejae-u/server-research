using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using UnityEngine;
using Cysharp.Threading.Tasks;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using NetworkData;
using Random = UnityEngine.Random;

public class AIManager : MonoBehaviour
{
    public bool IsMoving { get; private set; }
    private RpcPacket _rpcPacket;
    private NetworkManager _networkManager;
    

    private void Start()
    {
        _rpcPacket = new RpcPacket
        {
            Method = RpcMethod.None,
            Data = ByteString.Empty
        };
        
        _networkManager = NetworkManager.Instance;
        _rpcPacket.Uuid = NetworkManager.Instance.GetInstanceID().ToString();
    }

    public void MoveCharacter(Vector3 targetPosition)
    {
        if (IsMoving || !_networkManager.IsOnline)
            return;
        
        IsMoving = true;
        MoveCharacterAsync(targetPosition).Forget();
    }
    
    private async UniTask MoveCharacterAsync(Vector3 targetPosition)
    {
        float duration = Random.Range(1.5f, 5.0f);
        
        _rpcPacket.Method = RpcMethod.Move;
        
        // Serialize the target position to a byte array
        var serializePositionData = new PositionData
        {
            // Start position
            X1 = transform.position.x,
            Y1 = transform.position.y,
            Z1 = transform.position.z,
            
            // Target position
            X2 = targetPosition.x,
            Y2 = targetPosition.y,
            Z2 = targetPosition.z,
            
            Duration = duration // can deprecate this
        };
        
        _rpcPacket.Data = serializePositionData.ToByteString();
        _rpcPacket.Timestamp = Timestamp.FromDateTime(DateTime.UtcNow);

        NetworkManager.Instance.SendAsync(_rpcPacket).Forget();
        CancellationToken token = this.GetCancellationTokenOnDestroy();
        
        var elapsedTime = 0f;
        Vector3 startPosition = transform.position;

        while (elapsedTime < duration)
        {
            float t = elapsedTime / duration;
            transform.position = Vector3.Lerp(startPosition, targetPosition, t);
            transform.rotation = Quaternion.Slerp(transform.rotation, Quaternion.LookRotation(targetPosition - startPosition), t);
            
            await UniTask.Yield(PlayerLoopTiming.Update, token);
            elapsedTime += Time.deltaTime;
        }
        
        transform.position = targetPosition;
        IsMoving = false;
    }
}
        
        