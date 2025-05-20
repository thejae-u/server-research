using System;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using NetworkData;
using Unity.Behavior;
using UnityEngine;
using Action = Unity.Behavior.Action;
using Unity.Properties;
using Random = UnityEngine.Random;

[Serializable, GeneratePropertyBag]
[NodeDescription(name: "SendPacket", story: "Notify to NetworkManager And Move [AiManager]", category: "Action", id: "35c819287347764da93b47dfce107cbd")]
public partial class SendPacketAction : Action
{
    [SerializeReference] public BlackboardVariable<AIManager> AiManager;
    private RpcPacket _sendPacket;
    private Vector3 _targetPosition;
    private NetworkManager _networkManager;
    
    protected override Status OnStart()
    {
        _networkManager = NetworkManager.Instance;
        
        var startPosition =
            AiManager.Value.transform.position;
        var targetPosition =
            new Vector3(Random.Range(-5, 6), 1, Random.Range(-5, 6)); // Replace with actual target position
        _targetPosition = targetPosition;
        
        var positionData = new PositionData
        {
            X1 = startPosition.x,
            Y1 = startPosition.y,
            Z1 = startPosition.z,
            X2 = targetPosition.x,
            Y2 = targetPosition.y,
            Z2 = targetPosition.z
        };
        
        _sendPacket = new RpcPacket
        {
            Method = RpcMethod.Move,
            Data = positionData.ToByteString(),
            Timestamp = Timestamp.FromDateTime(DateTime.UtcNow),
        };
        
        return Status.Running;
    }

    protected override Status OnUpdate()
    {
        AiManager.Value.MoveTo(_targetPosition);
        _ = NetworkManager.Instance.AsyncWriteRpcPacket(_sendPacket);
        return Status.Success;
    }

    protected override void OnEnd()
    {
    }
}

