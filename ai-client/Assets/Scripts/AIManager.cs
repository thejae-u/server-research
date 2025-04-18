using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class AIManager : MonoBehaviour
{
    public bool IsMoving => _moveRoutine != null;
    private IEnumerator _moveRoutine;

    public void MoveCharacter(Vector3 targetPosition)
    {
        if (_moveRoutine != null)
            return;

        _moveRoutine = MoveToPosition(targetPosition);
        StartCoroutine(_moveRoutine);
    }

    private IEnumerator MoveToPosition(Vector3 targetPosition)
    {
        float duration = Random.Range(0.5f, 10.0f);
        float elapsedTime = 0.0f;
        
        Debug.Log($"Moving to {targetPosition} over {duration} seconds");

        while (elapsedTime < duration)
        {
            float t = elapsedTime / duration;
            Vector3 newPosition = Vector3.Lerp(transform.position, targetPosition, t);
            transform.position = newPosition;
            
            yield return null;
            elapsedTime += Time.deltaTime;
        }
        
        _moveRoutine = null;
    }
}
        
        