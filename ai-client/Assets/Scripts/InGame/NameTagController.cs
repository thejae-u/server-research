using System;
using Network;
using TMPro;
using UnityEngine;
using NetworkData;

public class NameTagController : MonoBehaviour
{
    private UserSimpleDto _user { get; set; }
    private GameObject _syncObject;
    private Transform _syncObjectTransform;
    private Camera _camera;

    private TMP_Text _nameTagText;
    private GameObject _nameTag;

    public void Init(UserSimpleDto user, GameObject syncObject)
    {
        _user = user;
        _syncObject = syncObject;
        _syncObjectTransform = syncObject.transform;

        _camera = Camera.main;

        _nameTagText = transform.GetComponentInChildren<TMP_Text>();
        _nameTagText.text = _user.Username;
        _nameTag = transform.GetChild(0).gameObject;
    }

    private void Update()
    {
        if (!IsVisible())
        {
            _nameTag.SetActive(false);
            return;
        }

        _nameTag.SetActive(true);
        Vector3 screenPoint = _camera.WorldToScreenPoint(_syncObjectTransform.position);
        screenPoint.y += 50; // Offset the name tag above the object
        _nameTag.transform.position = screenPoint;
        _nameTag.transform.Rotate(0, -_nameTag.transform.rotation.eulerAngles.y, 0); // Rotate the name tag to face the camera
    }

    private bool IsVisible()
    {
        if (!_syncObject)
        {
            Destroy(gameObject);
        }

        if (Guid.Parse(_user.Uid) == AuthManager.Instance.UserGuid)
            return false;

        Vector3 viewportPoint = _camera.WorldToViewportPoint(_syncObjectTransform.position);

        return viewportPoint.x is > 0.0f and < 1.0f
               && viewportPoint.y is > 0.0f and < 1.0f;
    }
}