using System;
using UnityEngine;
using UnityEngine.InputSystem;

[CreateAssetMenu(fileName = "PlayerMoveActions", menuName = "Scriptable Objects/PlayerMoveActions")]
public class PlayerMoveActions : ScriptableObject, IA_Base.IPlayerActions
{
    public Action onMoveStartAction;
    public Action<Vector2> onMoveAction;
    public Action onMoveStopAction;
    private Vector2 _lastMoveInput = Vector2.zero;

    public Action onAttackAction;
    
    private IA_Base _iaBase;
    private void OnEnable()
    {
        if (_iaBase == null)
        {
            _iaBase = new IA_Base();
            _iaBase.Player.SetCallbacks(this);
        }

        _iaBase.Player.Enable();
    }
    
    private void OnDisable()
    {
        _iaBase.Player.Disable();

        if (_iaBase == null) return;
        _iaBase.Player.SetCallbacks(null);
        _iaBase = null;

        onMoveStartAction = null;
        onMoveAction = null;
        onMoveStopAction = null;
        onAttackAction = null;
    }

    public void OnMove(InputAction.CallbackContext context)
    {
        if (context.canceled)
        {
            if(_lastMoveInput != Vector2.zero)
            {
                onMoveStopAction?.Invoke();
                _lastMoveInput = Vector2.zero;
            }

            return;
        }

        if (context.performed)
        {
            var input = context.ReadValue<Vector2>();

            if (input == Vector2.zero)
            {
                if (_lastMoveInput != Vector2.zero)
                {
                    onMoveStopAction?.Invoke();
                    _lastMoveInput = Vector2.zero;
                }
            }
            else
            {
                if (_lastMoveInput == Vector2.zero)
                {
                    onMoveStartAction?.Invoke();
                }

                if (input != _lastMoveInput)
                {
                    onMoveAction?.Invoke(input);
                    _lastMoveInput = input;
                }
            }
        }
    }

    public void OnAttack(InputAction.CallbackContext context)
    {
        if(!context.performed)
            return;

        onAttackAction?.Invoke();
    }
}
