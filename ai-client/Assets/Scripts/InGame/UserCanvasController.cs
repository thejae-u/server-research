using Network;
using UnityEngine;
using UnityEngine.UI;

public class UserCanvasController : MonoBehaviour
{
    private Button _backButton;

    private void Awake()
    {
        _backButton = transform.GetChild(0).GetComponent<Button>();
    }

    private void Start()
    {
        _backButton.onClick.AddListener(OnClickBackButton);
    }

    private void OnClickBackButton()
    {
        // Disconnect From Logic Server
        LogicServerConnector.Instance.disconnectAction?.Invoke();
    }
}