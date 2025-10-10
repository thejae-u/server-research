using System;
using Network;
using TMPro;
using UnityEngine;

public class NameTagController : MonoBehaviour
{
    private Guid ObjectId { get; set; }
    private GameObject _syncObject;
    private Transform _syncObjectTransform;
    private Camera _camera;

    private TMP_Text _nameTagText;
    private GameObject _nameTag;

    public void Init(Guid objectId, GameObject syncObject)
    {
        ObjectId = objectId;
        _syncObject = syncObject;
        _syncObjectTransform = syncObject.transform;
        
        _camera = Camera.main;
        
        _nameTagText = transform.GetComponentInChildren<TMP_Text>();
        Debug.Assert(_nameTagText is not null, "NameTagController.Init - _nameTagText is null");
        _nameTagText.text = ObjectId.ToString();
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

        if (ObjectId == NetworkManager.Instance.ConnectedUuid)
            return false;
        
        Vector3 viewportPoint = _camera.WorldToViewportPoint(_syncObjectTransform.position);
        
        return viewportPoint.x is > 0.0f and < 1.0f 
               && viewportPoint.y is > 0.0f and < 1.0f;
    }
}
