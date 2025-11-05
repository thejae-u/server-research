using System;
using System.Collections;
using System.Collections.Concurrent;
using UnityEngine;

public class MainThreadDispatcher : Singleton<MainThreadDispatcher>
{
    private static readonly ConcurrentQueue<Action> _jobQueue = new();

    private void Update()
    {
        if (!_jobQueue.TryDequeue(out var action))
            return;

        action.Invoke();
    }

    public void Enqueue(Action action)
    {
        _jobQueue.Enqueue(action);
    }

    public void Enqueue(IEnumerator coroutine)
    {
        _jobQueue.Enqueue(() => { StartCoroutine(coroutine); });
    }
}
