using System;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;
using Utility;

public class LobbyCanvasController : MonoBehaviour
{
    private GameObject _lobbyMainPanel;
    private GameObject _roomPanel;

    private Button _backToLoginPageButton;
    private Task _backToLoginPageTask;

    private void Awake()
    {
        _lobbyMainPanel = transform.GetChild(1).gameObject;

        _backToLoginPageButton = transform.GetChild(2).GetComponent<Button>();
        _backToLoginPageButton.onClick.AddListener(OnClickBackToLoginPageButton);
        
        _roomPanel = transform.GetChild(3).gameObject;
    }

    public void ChangePanelToRoomPanel(GroupDto groupDto)
    {
        _lobbyMainPanel.SetActive(false);
        _roomPanel.SetActive(true);
        _roomPanel.GetComponent<InRoomManager>().InitRoom(groupDto);
    }

    public void ChangePanelToLobbyPanel()
    {
        _roomPanel.SetActive(false);
        _lobbyMainPanel.SetActive(true);
    }
    
    private void OnClickBackToLoginPageButton()
    {
        if (_backToLoginPageTask is not null)
            return;
        
        _backToLoginPageTask = BackToLoginPageAsync();
    }

    private async Task BackToLoginPageAsync()
    {
        AsyncOperation op = SceneManager.LoadSceneAsync("LoginScene");
        while (op is { isDone: false })
        {
            await Task.Yield();
        }
    }
}