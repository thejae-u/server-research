using System;
using System.Threading;
using Cysharp.Threading.Tasks;
using NetworkData;
using UnityEngine;

public class SyncObject : MonoBehaviour
{
    // Several Sync Data can be added to the SyncObject
    public Guid ObjectId { get; private set; }
    private NetworkManager _networkManager;
    private MeshRenderer _meshRenderer;

    public void Init(Guid objectId)
    {
        ObjectId = objectId;
        _meshRenderer = GetComponent<MeshRenderer>();
        _networkManager = NetworkManager.Instance;

        if (ObjectId == _networkManager.ConnectedUuid)
        {
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);
        }
    }

    public async UniTask SyncPosition(Guid instanceGuid, MoveData moveData)
    {
        var startPosition = new Vector3(moveData.X, moveData.Y, moveData.Z);
        Vector3 direction = (transform.right * moveData.Horizontal + transform.forward * moveData.Vertical).normalized;

        float speed = 5.0f * Time.deltaTime;

        Vector3 targetPosition = startPosition + direction * speed;
        transform.position = targetPosition;

        Debug.Log($"{instanceGuid} : Move Data - Position: {moveData.X} {moveData.Y} {moveData.Z}, speed - {moveData.Speed}");
        Debug.Log($"Diff: {(moveData.X - transform.position.x)}, {(moveData.Y - transform.position.y)}, {(moveData.Z - transform.position.z)}");
        
        if(!Mathf.Approximately(moveData.X, transform.position.x) || !Mathf.Approximately(moveData.Y, transform.position.y) || !Mathf.Approximately(moveData.Z, transform.position.z))
            Debug.Log($"Need Interpolation: {ObjectId}");
        
        // Move the object to the target position
        await UniTask.Yield();
    }
}
