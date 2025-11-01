using System.Collections;
using UnityEngine;
using UnityEngine.SceneManagement;

public class SceneController : Singleton<SceneController>
{
    public enum EScene
    {
        LoginScene,
        LobbyScene,
        GameScene
    }

    private IEnumerator _loadSceneRoutine = null;

    public void LoadSceneAsync(EScene scene)
    {
        if (_loadSceneRoutine is not null)
            return;

        _loadSceneRoutine = LoadSceneRoutine(scene.ToString());
        StartCoroutine(_loadSceneRoutine);
    }

    private IEnumerator LoadSceneRoutine(string sceneName)
    {
        Debug.Log($"Scene Load Start");
        var op = SceneManager.LoadSceneAsync(sceneName);

        while (!op.isDone)
        {
            yield return null;
        }

        _loadSceneRoutine = null;
        Debug.Log($"Scene Load end");
    }
}