using System.Threading;
using Cysharp.Threading.Tasks;
using UnityEngine;

public class SyncObject : MonoBehaviour
{
    // Several Sync Data can be added to the SyncObject
    
    public string ObjectId { get; private set; }

    private bool _isPositionSyncing;
    
    public void Init(string objectId)
    {
        ObjectId = objectId;
    }
    
    public void SyncPosition(Vector3 startPosition, Vector3 targetPosition, float duration)
    {
        if (_isPositionSyncing)
            return;
        
        _isPositionSyncing = true;
        MoveObject(startPosition, targetPosition, duration).Forget();
    }

    private async UniTask MoveObject(Vector3 startPosition, Vector3 targetPosition, float duration)
    {
        CancellationToken token = this.GetCancellationTokenOnDestroy();
        
        transform.position = startPosition;
        var elapsedTime = 0f;

        while (elapsedTime < duration)
        {
            float t = elapsedTime / duration;
            transform.position = Vector3.Lerp(startPosition, targetPosition, t);
            transform.rotation = Quaternion.Slerp(transform.rotation, Quaternion.LookRotation(targetPosition - startPosition), t);
            
            await UniTask.Yield(PlayerLoopTiming.Update, token);
            elapsedTime += Time.deltaTime;
        }

        transform.position = targetPosition;
        _isPositionSyncing = false;
    }
}
