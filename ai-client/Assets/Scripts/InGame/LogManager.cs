using UnityEngine;
using System.Collections.Generic;
using System.Text;
using System;

public class LogManager : MonoBehaviour
{
    private List<string> _logs = new List<string>();
    private StringBuilder _logBuilder = new StringBuilder();

    private Vector2 _scrollPosition = Vector2.zero;
    private float _windowWidth = 720f;
    private float _windowHeight = 500f;
    private float margin = 10f;

    private int _maxLogLine = 50;

    private static LogManager _instance;
    public static LogManager Instance
    {
        get 
        {
            if (_instance is not null)
                return _instance;

            _instance = FindFirstObjectByType<LogManager>();
            return _instance;
        }
    }

    private void Awake()
    {
        if (_instance is not null && _instance != this)
            Destroy(gameObject);
    }

    public void Log(string message)
    {
        _logs.Add($"{DateTime.UtcNow.ToLocalTime()} {message}");

        if (_maxLogLine > 0 && _logs.Count > _maxLogLine)
        {
            _logs.RemoveAt(0);
        }

        _logBuilder.Clear();
        foreach (string log in _logs)
        {
            _logBuilder.AppendLine(log);
        }

        _scrollPosition.y = float.MaxValue;
    }

    void OnGUI()
    {

        Rect windowRect = new Rect(
            Screen.width - _windowWidth - margin,
            Screen.height - _windowHeight - margin,
            _windowWidth,
            _windowHeight
        );

        GUI.Box(windowRect, "Logs");

        Rect viewRect = new Rect(
            windowRect.x + 10,
            windowRect.y + 25, 
            windowRect.width - 20,
            windowRect.height - 35
        );


        GUIContent content = new GUIContent(_logBuilder.ToString());
        GUIStyle style = GUI.skin.textArea;
        style.wordWrap = true;

        float innerHeight = style.CalcHeight(content, viewRect.width - 20);
        Rect contentRect = new Rect(0, 0, viewRect.width - 20, innerHeight);

        _scrollPosition = GUI.BeginScrollView(viewRect, _scrollPosition, contentRect);

        GUI.TextArea(contentRect, _logBuilder.ToString(), style);

        GUI.EndScrollView();
    }
}