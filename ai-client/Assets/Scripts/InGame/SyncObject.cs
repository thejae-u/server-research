using System;
using System.Threading;
using Cysharp.Threading.Tasks;
using NetworkData;
using UnityEngine;

public class SyncObject : MonoBehaviour
{
    [SerializeField] private PlayerStatData _playerStatData; // Player Stat Data for the object
    
    // Several Sync Data can be added to the SyncObject
    public Guid ObjectId { get; private set; }
    private NetworkManager _networkManager;
    private MeshRenderer _meshRenderer;

    private readonly object _positionLock = new();
    private Vector3 _lastNetworkPosition;

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

        float speed = _playerStatData.speed * Time.deltaTime;

        lock (_positionLock)
        {
            _lastNetworkPosition = startPosition + direction * speed;
        }

        Debug.Log($"{instanceGuid} : Move Data - Position: {moveData.X} {moveData.Y} {moveData.Z}, speed - {moveData.Speed}");
        Debug.Log($"Diff: {(moveData.X - transform.position.x)}, {(moveData.Y - transform.position.y)}, {(moveData.Z - transform.position.z)}");
        
        if(!Mathf.Approximately(moveData.X, transform.position.x) || !Mathf.Approximately(moveData.Y, transform.position.y) || !Mathf.Approximately(moveData.Z, transform.position.z))
            Debug.Log($"Need Interpolation: {ObjectId}");
        
        // Move the object to the target position
        await UniTask.Yield();
    }

    private void Update()
    {
        // interpolate the position of the object
        if (!_networkManager.IsOnline)
            return;
        
        lock (_positionLock)
        {
            if (Vector3.Distance(transform.position, _lastNetworkPosition) > 0.01f)
            {
                transform.position = Vector3.Lerp(transform.position, _lastNetworkPosition, Time.deltaTime * _playerStatData.speed);
            }
        }
    }
}
