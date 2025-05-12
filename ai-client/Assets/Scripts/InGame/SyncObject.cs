using System;
using System.Threading;
using Cysharp.Threading.Tasks;
using UnityEngine;

public class SyncObject : MonoBehaviour
{
    // Several Sync Data can be added to the SyncObject
    public Guid ObjectId { get; private set; }

    public void Init(Guid objectId)
    {
        ObjectId = objectId;
    }

    public void SyncPosition(Guid instanceGuid, Vector3 startPosition, Vector3 targetPosition)
    {
        Debug.Log($"{instanceGuid.ToString()} - SyncObject.SyncPosition - {startPosition} to {targetPosition}");
    }
}
