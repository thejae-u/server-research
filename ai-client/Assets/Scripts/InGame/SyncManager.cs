using System;
using UnityEngine;
using System.Collections.Generic;
using Cysharp.Threading.Tasks;

public class SyncManager : Singleton<SyncManager> 
{
    [SerializeField] private GameObject _syncObjectPrefab;
    [SerializeField] private GameObject _syncObjectNameTagPrefab;
    [SerializeField] private Transform _syncObjectCanvas;
    
    private NetworkManager _networkManager;
    
    private Dictionary<Guid, GameObject> _syncObjects = new();

    private void OnEnable()
    {
        NetworkManager.Instance.disconnectAction += OnDisconnected;
    }
    
    private void OnDisable()
    {
    }

    private void Start()
    {
        _networkManager = NetworkManager.Instance;
    }

    private void OnDisconnected()
    {
        // all sync objects should be removed
        foreach (GameObject syncObject in _syncObjects.Values)
        {
            Destroy(syncObject);
        }
    }

    private GameObject CreateSyncObject(Guid objectId, Vector3 position) 
    {
        GameObject syncObject = Instantiate(_syncObjectPrefab, position, Quaternion.identity); // Create the object
        
        syncObject.transform.SetParent(transform); // Set parent to SyncManager (here)
        syncObject.name = objectId.ToString(); // Set the name to the objectId
        syncObject.GetComponent<SyncObject>().Init(objectId); // Initialize SyncObject
        
        // Create the name tag
        GameObject nameTag = Instantiate(_syncObjectNameTagPrefab, _syncObjectCanvas);
        nameTag.GetComponent<NameTagController>().Init(objectId, syncObject);
        
        _syncObjects.Add(objectId, syncObject); // Add to dict

        return syncObject;
    }

    public void SyncObjectPosition(Guid objectId, Vector3 startPosition, Vector3 targetPosition)
    {
        // self packet check and return -> TODO : interporlation first move to sync with server
        if (objectId == _networkManager.ConnectedUuid)
            return;
        
        if (!_syncObjects.TryGetValue(objectId, out GameObject syncObject))
        {
            // If the object doesn't exist, create it
            syncObject = CreateSyncObject(objectId, startPosition);
        }
        
        var syncObjectComponent = syncObject.GetComponent<SyncObject>();
        // Sync position
        syncObjectComponent.SyncPosition(objectId, startPosition, targetPosition).Forget();
    }
}
