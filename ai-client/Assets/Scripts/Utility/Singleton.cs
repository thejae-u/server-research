using System;
using UnityEngine;

public class Singleton<T> : MonoBehaviour where T : MonoBehaviour
{
    private static T _instance;

    public static T Instance
    {
        get
        {
            if (_instance != null)
            {
                return _instance;
            }
            
            _instance = FindFirstObjectByType<T>();
            DontDestroyOnLoad(_instance);
            return _instance;
        }
    }

    private void Awake()
    {
        if(_instance != null && _instance != this)
        {
            Destroy(gameObject);
            return;
        }

        if (!TryGetComponent(out _instance))
        {
            Debug.Assert(false, $"Singleton<{typeof(T).Name}> not found");
            return;
        }
        
        DontDestroyOnLoad(_instance);
    }
}
