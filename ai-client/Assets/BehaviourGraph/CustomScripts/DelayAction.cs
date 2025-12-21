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
    protected override Status OnStart()
    {
        return Status.Failure;
    }

    protected override Status OnUpdate()
    {
        return Status.Failure;
    }

    protected override void OnEnd()
    {
    }
}