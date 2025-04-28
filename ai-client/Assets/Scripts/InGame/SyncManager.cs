using UnityEngine;
using System.Collections.Generic;
using Cysharp.Threading.Tasks;

public class SyncManager : Singleton<SyncManager> 
{
    [SerializeField] private GameObject _syncObjectPrefab;
    private Dictionary<string, GameObject> _syncObjects = new();

    private GameObject CreateSyncObject(string objectId, Vector3 position) 
    {
        GameObject syncObject = Instantiate(_syncObjectPrefab, position, Quaternion.identity); // Create the object
        
        syncObject.transform.SetParent(transform); // Set parent to SyncManager (here)
        syncObject.name = objectId; // Set the name to the objectId
        syncObject.GetComponent<SyncObject>().Init(objectId); // Initialize SyncObject
        
        _syncObjects.Add(objectId, syncObject); // Add to dict

        return syncObject;
    }

    public void SyncObjectPosition(string objectId, Vector3 startPosition, Vector3 targetPosition, float duration)
    {
        if (!_syncObjects.TryGetValue(objectId, out GameObject syncObject))
        {
            // If the object doesn't exist, create it
            syncObject = CreateSyncObject(objectId, startPosition);
        }
        
        // Sync position
        syncObject.GetComponent<SyncObject>().SyncPosition(startPosition, targetPosition, duration);
    }
}
