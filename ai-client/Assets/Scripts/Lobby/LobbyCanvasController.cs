using System;
using UnityEngine;

public class LobbyCanvasController : MonoBehaviour
{
    private GameObject _lobbyMainPanel;
    private GameObject _roomPanel;

    private void Awake()
    {
        _lobbyMainPanel = transform.GetChild(1).gameObject;
        _roomPanel = transform.GetChild(2).gameObject;
    }

    public void ChangePanelToRoomPanel()
    {
        _lobbyMainPanel.SetActive(false);
        _roomPanel.SetActive(true);
    }

    public void ChangePanelToLobbyPanel()
    {
        _roomPanel.SetActive(false);
        _lobbyMainPanel.SetActive(true);
    }
}