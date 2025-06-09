using System;
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
    
    private bool _isMyObject => ObjectId == _networkManager.ConnectedUuid;

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

    public async UniTask SyncPosition(MoveData moveData)
    {
        // Start position at the time of input
        var startPosition = new Vector3(moveData.X, moveData.Y, moveData.Z); 
        
        // moved direction (key input value)
        Vector3 direction = (transform.right * moveData.Horizontal + transform.forward * moveData.Vertical).normalized;

        // calculate new position by moveData
        float speed = _playerStatData.speed * Time.deltaTime;

        lock (_positionLock)
        {
            _lastNetworkPosition = startPosition + direction * speed;
        }
        
        await UniTask.Yield();
    }

    private void Update()
    {
        // interpolate the position of the object
        if (!_networkManager.IsOnline)
            return;

        if (Vector3.Distance(transform.position, _lastNetworkPosition) < 0.01f)
        {
            if (_isMyObject)
                _meshRenderer.material.color = Color.clear;
            return;
        }

        if (_isMyObject)
        {
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);
        }
        
        Vector3 lastPosition;
        lock (_positionLock)
        {
            lastPosition = _lastNetworkPosition;
        }

        transform.position =
            Vector3.Lerp(transform.position, lastPosition, Time.deltaTime * _playerStatData.speed);
    }
}
