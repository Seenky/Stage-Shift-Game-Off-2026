# Театральный хаб Stage Shift

Карта находится в `/Game/LVL/Hub/LVL_Hub`. В Outliner секции `Hub/01 Architecture`–`Hub/08 Rigging` разбиты на папки по зонам. Предметы можно выделять и двигать по одному. Пол, стены, ступени и перила используют коллизию по форме. На крупной мебели стоят простые коллизии, которые двигаются вместе с предметом.

## Правка декораций

Из папки `Models/Hub` обновите экспорт. Затем выберите нужный меш в `/Game/LVL/Hub/Theatre/HubTheatre/Meshes` и нажмите **Reimport**. Сохраните имя FBX и слоты материалов. Исходные пивоты и текстурные атласы описаны в `Surfacing/HubTheatre/Manifest.json`.

```powershell
blender --background Hub_Theatre.blend --python Surfacing/HubTheatre/export_source.py
```

Для независимых предметов в `Meshes/Modular` используйте `Surfacing/HubTheatre/export_modules.py`. Он экспортирует FBX и пивоты в `Surfacing/HubTheatre/Modular` с UV исходного атласа. Перестановки предметов в Unreal сохраняются в самой карте. При обновлении отдельного предмета достаточно реимпорта его FBX.

Переход в репетиционную использует отдельный исходник `Surfacing/HubTheatre/Modular/Source/Hub_Rehearsal_Threshold.blend` и `SM_Hub_Rehearsal_Threshold.fbx`. Доски и подложка используют UV действующего атласа настила `MI_Hub_PlankFloor`. При реимпорте сохраняйте слот `Hub_PlankFloor`, исходные нормали и выпуклую коллизию `UCX_SM_Hub_Rehearsal_Threshold_00`.

Карнизы и полки фонарей репетиционной собираются в `Surfacing/HubTheatre/Modular/Source/Hub_Rehearsal_Hardware.blend`. В `Modular/Rehearsal.json` указаны размеры поставки и материалы слотов. Шторы прикреплены к своим карнизам, а фонари к полкам. Перемещая крепление в Unreal, сверяйте контакт подвесов с кромкой ткани и крепёжную плоскость с деревом.

Подиум остаётся корневым мешем `Stage Selection Station`. Рама, стойки и рельсы вынесены в отдельные акторы. Сменные декорации используют общую точку привязки и переключаются цельными кассетами.

## Выбор постановки

В Details актора `Stage Selection Station` массив **Options** задаёт порядок, название, меш декорации, карту и доступность. Для запуска нужны включённый **Unlocked** и выбранный **Level**. Камеру приближения задаёт **Selection Camera**, а подписи и оформление панели редактируются в `/Game/LVL/Hub/UI/W_StageSelection`.

ЛКМ или RB у стенда открывают выбор. Стрелки или горизонталь левого стика листают постановки. Enter или A запускают доступную постановку. Esc или B закрывают выбор.

## Текстуры игрового хаба

`LVL_Hub` использует модули из `Surfacing/HubTheatre/FBX` и экземпляры `M_Surface` из `/Game/LVL/Hub/Theatre/HubTheatre/Materials`. Для художественного ревью открывайте `Hub_Theatre.blend` с упакованными картами из `Surfacing/HubTheatre/Textures`. Четыре проекта [HubTheatre в Painter](Surfacing/HubTheatre/Painter) используют UV этой же сцены.

- Все 25 активных наборов экспортируются в 2048 × 2048. Пресеты Flat Shaded `0.20` и Hand Painted `1.0` с Baked Lighting `0.40` включены во всех наборах. Поверх стилизации отдельные слои задают рисунок картона, направленные волокна дерева и вышивку.
- Веточки на вывеске собраны из встроенной альфы Painter `Ornament Leaf` в наборе `Hub_SelectorSign`. У зелёной панели свой UV-атлас.
- [Референсы и кадры Blender](Surfacing/HubTheatre/References) показывают общую композицию, вывеску, панель управления и проработку материалов.

Исходники предыдущей ревизии для запекания сохранены в `FBX` и `Surfacing/Painter`. Её опубликованные FBX из `Surfacing/Runtime` и скрипт импорта перенесены в архив очистки. Для текущей ревизии используйте `Surfacing/HubTheatre` и проверяйте путь импорта на нужном ассете в Unreal.

Нейтральная карта нормалей для вывески используется также в меню и хранится в `/Game/MATERIALS/PBR/T_StageShiftFlatNormal`. Общие ассеты перечислены в [каталоге окружения](../Environment/README.md). Требования к материалам описаны в [арт-гайде](../../Docs/ArtDirection.md).
