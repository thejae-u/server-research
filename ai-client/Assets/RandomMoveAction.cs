using System;
using Unity.Behavior;
using UnityEngine;
using Action = Unity.Behavior.Action;
using Unity.Properties;
using UnityEngine.Serialization;
using Random = UnityEngine.Random;

[Serializable, GeneratePropertyBag]
[NodeDescription(name: "Random Move", story: "[Transform] [aiManager] Randomly", category: "Action", id: "c035123eee9696b0faca4c1e609add2f")]
public partial class RandomMoveAction : Action
{
    [SerializeReference] public BlackboardVariable<Transform> aiTransform;
    [SerializeReference] public BlackboardVariable<AIManager> aiManager;
    private Vector3 _targetPosition;

    protected override Status OnStart()
    {
        return Status.Running;
    }

    protected override Status OnUpdate()
    {
        Vector3 newPosition = new Vector3(Random.Range(-10f, 10f), aiTransform.Value.position.y, Random.Range(-10f, 10f));
        
        if (aiManager.Value.IsMoving)
        {
            return Status.Running;
        }
        
        aiManager.Value.MoveCharacter(newPosition);
        
        // Send Move Packet to Server
        // SendToServer(newPosition);
        
        return Status.Success;
    }

    protected override void OnEnd()
    {
    }
}

