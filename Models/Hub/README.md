# Театральный хаб Stage Shift

Карта находится в `/Game/LVL/Hub/LVL_Hub`. Исходник `Hub_Theatre.blend` разбит на коллекции по модулям. В Unreal каждый FBX импортируется в отдельный Static Mesh под `/Game/LVL/Hub/Theatre/Meshes`. Проходы и пол используют отдельные коллизии в папке `Hub/09 Collision`.

## Правка декораций

Из папки `Models/Hub` экспортируйте нужную коллекцию. Затем выберите меш в Unreal и нажмите **Reimport**. Сохраните имя FBX и слоты материалов. Точки привязки хранятся в `Hub_Theatre.json`.

```powershell
blender --background Hub_Theatre.blend --python ../Menu/Export_FBX.py -- CrimePreview
```

`SelectionStand` содержит раму и подиум. Рельсы и боковые кулисы находятся в `SceneryRails`. Сменные декорации `CrimePreview`, `WinterPreview` и `SeaPreview` имеют общую точку привязки.

## Выбор постановки

В Details актора `Stage Selection Station` массив **Options** задаёт порядок, название, меш декорации, карту и доступность. Для запуска нужны включённый **Unlocked** и выбранный **Level**. Камеру приближения задаёт **Selection Camera**, а подписи и оформление панели редактируются в `/Game/LVL/Hub/UI/W_StageSelection`.

ЛКМ или RB у стенда открывают выбор. Стрелки или горизонталь левого стика листают постановки. Enter или A запускают доступную постановку. Esc или B закрывают выбор.
