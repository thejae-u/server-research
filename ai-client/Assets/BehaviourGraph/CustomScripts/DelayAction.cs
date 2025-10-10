using System;
using Network;
using Unity.Behavior;
using UnityEngine;
using Action = Unity.Behavior.Action;
using Unity.Properties;
using Random = UnityEngine.Random;

[Serializable, GeneratePropertyBag]
[NodeDescription(name: "Delay", story: "Wait Randomly [includeA] to [includeB]", category: "Action/Delay", id: "634d635933dc9a6d1ef1312fb021373b")]
public partial class DelayAction : Action
{
    [SerializeReference] public BlackboardVariable<int> IncludeA;
    [SerializeReference] public BlackboardVariable<int> IncludeB;
    
    private float _delayTime;
    private float _elapsedTime;

    private NetworkManager _networkManager;

    protected override Status OnStart()
    {
        _networkManager = NetworkManager.Instance;
        
        if(_networkManager.IsManualMode)
            return Status.Failure;
        
        int min = IncludeA.Value;
        int max = IncludeB.Value;
        _delayTime = (float)Random.Range(min, max + 1) / 100;
        _elapsedTime = 0;
        
        return Status.Running;
    }

    protected override Status OnUpdate()
    {
        if (!_networkManager.IsSendPacketOn || !_networkManager.IsOnline)
        {
            return Status.Running;
        }
        
        _elapsedTime += Time.deltaTime;
        return _elapsedTime < _delayTime ? Status.Running : Status.Success;
    }

    protected override void OnEnd()
    {
    }
}

