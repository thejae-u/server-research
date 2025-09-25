using System;
using TMPro;
using UnityEngine;
using UnityEngine.UI;
using Utility;

public class RoomController : MonoBehaviour
{
    private GroupDto _groupDto;
    
    public Guid RoomGuid { get; private set; }
    private TMP_Text _roomNameText;
    private TMP_Text _roomOwnerNameText;
    private TMP_Text _roomPlayerCountText;
    
    private Button _joinButton;

    public void UpdateRoomState(GroupDto data)
    {
        _groupDto = data;
        
        RoomGuid = data.groupId;
        _roomNameText.text = data.name;
        _roomOwnerNameText.text = data.owner.username;
        _roomPlayerCountText.text = data.players.Count.ToString();
    }

    private void Awake()
    {
        _joinButton = transform.GetChild(transform.childCount - 1).GetComponent<Button>();
        _joinButton.onClick.AddListener(OnClickJoinButton);

        _roomNameText = transform.GetChild(0).GetComponent<TMP_Text>();
        _roomOwnerNameText = transform.GetChild(1).GetComponent<TMP_Text>();
        _roomPlayerCountText = transform.GetChild(2).GetComponent<TMP_Text>();
    }

    private void OnClickJoinButton()
    {
        // Room Guid로 web Server에 접근 요청
    }
}
