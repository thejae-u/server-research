using System;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Network;
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
    private LogicServerConnector _networkManager;

    protected override Status OnStart()
    {
        _networkManager = LogicServerConnector.Instance;

        float vertical = Random.Range(-1.0f, 1.0f);
        float horizontal = Random.Range(-1.0f, 1.0f);
        float speed = Random.Range(3.0f, 8.0f);

        Vector3 position = AiManager.Value.transform.position;

        var moveData = new MoveData
        {
            X = position.x,
            Y = position.y,
            Z = position.z,
            Vertical = vertical,
            Horizontal = horizontal,
            Speed = speed
        };

        _sendPacket = new RpcPacket
        {
            Method = RpcMethod.Move,
            Data = moveData.ToString(),
            Timestamp = Timestamp.FromDateTime(DateTime.UtcNow),
        };

        return Status.Running;
    }

    protected override Status OnUpdate()
    {
        // AiManager.Value.MoveTo();
        //_ = NetworkManager.Instance.AsyncWriteRpcPacket(_sendPacket);
        return Status.Success;
    }

    protected override void OnEnd()
    {
    }
}