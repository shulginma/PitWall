# PitWall 0.4.1

Исправлено обрезание окна при масштабировании Windows. Машина отмечена YOU.
Перед обновлением закрой старое окно PitWall. См. DPI_FIX_RU.md.

Теперь есть графическое окно симулятора. Начни с [GUI_STAGE_RU.md](GUI_STAGE_RU.md).

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
cmake --preset windows-mingw-gui
cmake --build --preset windows-mingw-gui --parallel 2
ctest --preset windows-mingw-gui
.\build\windows-mingw-gui\apps\simulator\pitwall_simulator.exe --drive
```

W — газ, S — тормоз, A/D — поворот, P — пауза, R — новый заезд, Escape — выход.
Для F5 в VS Code выбери PitWall: Drive (MinGW); нужно расширение C/C++.
Старые пресеты без суффикса -gui по-прежнему собирают консольную версию.
Проверены сборки и 68 тестов MinGW/MSVC и отрисовка настоящего кадра OpenGL.
Ручное управление и круг предстоит проверить в окне. Linux пока без графики.
История предыдущих этапов: NEXT_STAGE_RU.md (физика) и TRACK_STAGE_RU.md (трасса).
