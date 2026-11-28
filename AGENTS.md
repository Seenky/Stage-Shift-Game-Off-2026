## Обязательная приёмка для всех задач

Применяй [общие правила приёмки Stage Shift](Docs/Acceptance.md) к коду, геймплею, контенту, инструментам и документации. Принимай результат по фактической проверке в приложении или игровом сценарии, где его будут использовать. Профильные руководства и скиллы уточняют критерии. При обнаружении дефекта исправь его и повтори затронутые проверки. Указывай выполненные проверки и открытые ограничения.

## Общие требования к архитектуре и качеству кода

Перед внесением изменений изучи существующую реализацию, связанные системы, зависимости и принятые в проекте архитектурные решения.
Порядок работы с кодом, игровыми данными, исходниками и контентом описан в [руководстве по производству](Docs/ProductionWorkflow.md). Открывай разделы, относящиеся к задаче.

- **KISS, DRY, прагматичный SOLID.** Пиши простой, читаемый и поддерживаемый код. Избегай overengineering, ненужных абстракций, интерфейсов, менеджеров и дублирования.
- **Сохраняй существующую архитектуру.** Не переписывай работающие системы и не меняй публичные API без реальной необходимости. Предпочитай минимальные локальные изменения.
- **Соблюдай ответственность систем.** Новая функциональность должна находиться в логически подходящем месте. Не создавай God Classes, скрытые зависимости и дублирующиеся состояния.
- **Проектируй с учётом расширения.** Новые механики и контент должны добавляться преимущественно без изменения несвязанных систем. Но не реализуй функциональность на будущее без текущей необходимости.
- **Используй существующие решения.** Переиспользуй компоненты, утилиты и паттерны проекта. Новые классы создавай только при обоснованной потребности.
- **Определи владельца состояния.** Меняй состояние через существующего владельца механики. Виджеты показывают данные и передают действия игрока, сценарии задают порядок и условия, акторы и компоненты исполняют механику.
- **Настраивай контент данными.** Для постановок, сценических задач, реквизита и привязок используй существующие Data Assets, графы и настройки экземпляров акторов. Порядок конкретной постановки хранится в её данных.
- **Сохраняй обратную совместимость.** Не ломай существующее поведение, настройки, ассеты, сериализацию и зависимости. Не удаляй функциональность без необходимости.
- **Проверяй живые привязки.** Перед правкой C++, Blueprint, ассета или карты найди игровой экземпляр, его overrides и потребителей. При изменении карты сохраняй трансформы и настройки остальных акторов, включая внешние пакеты, если карта их использует.
- **Избегай усложнения настройки.** Параметры должны быть понятными, сгруппированными и иметь безопасные значения по умолчанию.
- **Проверяй результат.** После изменений проверяй затронутые зависимости, компиляцию и доступные тесты. Не утверждай, что проверки выполнены, если они не запускались.

**Главный принцип:** реализуй задачу минимальным количеством понятных изменений, сохраняя существующую функциональность и возможность дальнейшего расширения. Если для решения требуется существенное изменение архитектуры, сначала объясни его необходимость и последствия, не выполняя масштабный рефакторинг самостоятельно.

## Общие требования к Level Design, Lighting и Environment Art

При создании или изменении уровней придерживайся следующих принципов:

Для планировки, маршрутов, игровых камер и механизмов применяй [stage-shift-level-design](.agents/skills/stage-shift-level-design/SKILL.md). Для сборки окружения, расстановки, стыков и света применяй [stage-shift-level-art](.agents/skills/stage-shift-level-art/SKILL.md) вместе с `visual-quality-control`. [Правила сборки уровня](Docs/LevelDesignAndArt.md) обязательны для новых зон и правок существующих.

- **Понятная структура уровня.** Игрок должен быстро понимать, где он находится, куда может пройти, где расположены интерактивные объекты и какие зоны отвечают за разные игровые задачи.
- **Gameplay First.** Визуальное оформление должно поддерживать геймплей, а не мешать ему. Избегай узких проходов, неудобных препятствий, скрытых интерактивных объектов и лишнего визуального шума.
- **Хорошая композиция.** Уровень должен выглядеть гармонично с основной игровой камеры. Используй понятную визуальную иерархию, выразительные силуэты, композиционные акценты и свободное пространство вокруг важных объектов.
- **Приятное освещение.** Используй мягкий, художественно выстроенный свет с выразительными акцентами, естественными градиентами и контролируемым контрастом. Избегай пересвеченных участков, чрезмерно тёмных зон, грязных теней и плоского равномерного освещения.
- **Читаемость игровых объектов.** Персонажи, реквизит, механизмы и зоны взаимодействия должны хорошо различаться на фоне окружения. Освещение и цвета не должны скрывать важные элементы.
- **Визуальное разделение зон.** Используй свет, цвет, форму и детали окружения, чтобы игрок интуитивно различал игровые области и понимал их назначение.
- **Единый художественный стиль.** Для Stage Shift придерживайся эстетики миниатюрного handmade-театра: вязаные куклы, картонные декорации, дерево, ткани, кулисы и тёплая театральная атмосфера.
- **Осмысленная детализация.** Добавляй детали, которые создают атмосферу и помогают ориентироваться. Не перегружай уровень декоративными объектами, особенно в местах активного геймплея.
- **Камера определяет дизайн.** Проверяй композицию, размеры объектов, читаемость и освещение с реального игрового ракурса сверху под углом, а не только в свободной перспективе редактора.
- **Функциональность пространства.** Закулисье должно оставаться удобным для перемещения, подготовки декораций и работы с реквизитом. Зрители должны видеть только текущую сцену; остальные сектора вращающейся платформы и рабочие зоны необходимо скрывать театральными конструкциями.
- **Последовательность и переиспользование.** Используй общие принципы масштаба, освещения и организации пространства между уровнями, сохраняя индивидуальность каждой постановки.
- **Производительность.** Избегай неоправданно дорогого освещения, избыточной геометрии и эффектов. Оптимизируй только с учётом реальных ограничений и результатов профилирования.
- **Обязательная визуальная проверка.** После изменений проверяй уровень в игровом разрешении, с игровой камеры, при реальном освещении и во время движения персонажей. Исправляй проблемы читаемости, композиции и навигации.
- **Проверка конструкции.** Перед сохранением отдельно осмотри вблизи оба края каждого затронутого перехода, весь настил по ширине, опоры мебели, основания реквизита и крепления светильников. Проходимость по коллизии и визуальная непрерывность подтверждаются отдельно. Для механизмов бери класс и настройки из рабочей постановки, включая визуальный меш, коллизию и точки взаимодействия.

**Главный принцип:** каждый уровень должен быть визуально привлекательным, атмосферным и одновременно максимально понятным для игрока. Красота не должна достигаться ценой удобства геймплея, а функциональность — превращать уровень в пустое техническое пространство.

## Unreal Engine Material Graph — Structure & Readability Requirements

When creating, modifying, or extending any Unreal Engine Material or Material Function, maintain a clean, readable, and logically organized node graph from the very beginning.

**Graph readability and structure are mandatory requirements, not optional cleanup tasks.**

### 1. Graph Layout & Data Flow
- Organize all nodes consistently from left to right: **Inputs / Parameters → Calculations → Processing → Material Outputs**.
- Keep the primary data flow on a clear horizontal spine.
- Position supporting calculations, masks, and secondary branches above or below the main flow.
- Arrange nodes in logical execution order with consistent spacing and alignment.
- Avoid backward connections, excessive wire lengths, overlapping nodes, and unnecessary wire crossings.
- Ensure the graph remains readable at normal zoom levels.

### 2. Logical Grouping & Comments
- Group related nodes into clearly labeled comment boxes based on their responsibilities.
- Use concise, meaningful section names such as `UV Processing`, `Mask Generation`, `Surface Detail`, `Color Blending`, or `Final Output`.
- Each comment box should represent a cohesive operation or logical subsystem.
- Keep comment boxes aligned and visually separated.
- Avoid oversized comment boxes containing unrelated operations.
- Do not create comment boxes for trivial operations unless they improve understanding.

### 3. Named Reroutes
- Use **Named Reroute Declaration / Usage** nodes for values shared across multiple sections or referenced over long distances.
- Give named reroutes descriptive, unambiguous names reflecting their actual purpose, such as `BaseUV`, `WorldMask`, `BlendFactor`, or `SurfaceNormal`.
- Place declarations close to where values are produced.
- Place usage nodes close to their consumers.
- Prefer named reroutes over long wires crossing unrelated sections.
- Avoid excessive named reroutes for short, local connections.

### 4. Parameters & Inputs
- Group related parameters together near the beginning of their corresponding processing sections.
- Use consistent, descriptive parameter names.
- Organize exposed parameters into meaningful parameter groups where supported.
- Keep default values, ranges, types, and parameter semantics intentional and consistent.
- Avoid scattering related parameters throughout the graph.

### 5. Graph Consistency & Maintainability
- Apply the same organizational conventions across all Materials and Material Functions.
- Maintain visual consistency in node spacing, alignment, comment placement, and data-flow direction.
- Keep independent subsystems visually separated.
- When extending an existing graph, integrate new nodes into its logical structure rather than attaching disconnected clusters.
- Reorganize affected sections whenever necessary to preserve readability.
- Do not allow temporary, experimental, or disconnected nodes to accumulate in finalized graphs.

### 6. Behavior Preservation
- Graph organization must never change material behavior unless a functional modification is explicitly requested.
- When reorganizing an existing graph, preserve all calculations, parameters, constants, default values, material properties, and output connections.
- Do not introduce optimizations, redesigns, or functional changes as part of a layout-only operation.

### 7. Final Validation
Before considering any Material or Material Function complete:

- Verify that the graph follows a clear left-to-right structure.
- Ensure related operations are properly grouped and labeled.
- Check for overlapping nodes, unnecessary crossings, excessive wire lengths, and backward connections.
- Verify that shared values use named reroutes where appropriate.
- Confirm that all required nodes and output connections remain intact.
- Ensure the graph is understandable without manually tracing wires across the entire material.
- Validate that the Material compiles and retains its intended behavior.

**Important:** These requirements apply continuously throughout material creation and modification. Do not postpone graph organization until the end. Every newly created section must already follow these conventions.

## Визуальные задачи Stage Shift

Для моделинга, UV, текстур, материалов, риггинга, анимации, окружения, света, UI и VFX применяй `visual-quality-control`, если навык доступен. Требования проекта хранятся в [руководстве по производству](Docs/ProductionWorkflow.md) и [арт-гайде](Docs/ArtDirection.md), поэтому действуют в любом рабочем окружении.

- Перед моделированием проверь [общую библиотеку окружения](Models/Environment/Library.json), исходник локации и актуальный ассет в Unreal. Переиспользуй существующую геометрию, материалы и рабочие механизмы. Новая геометрия принадлежит общей коллекции при общем использовании или исходнику конкретной локации при локальном.
- Для визуальных задач сверяй форму и расстановку с референсом, а масштаб, материалы и уровень стилизации с ближайшим готовым ассетом или кадром Stage Shift. Проверяй объём, конструкцию и расстановку с игровой камеры до детализации. Сохраняй редактируемый исходник Blender, корректную топологию и UV, пивоты и слоты.
- Для финальных материалов используй только [библиотеку Stage Shift](Models/Materials/Painter/README.md) и рабочие стеки Painter проекта. Открой SPP игрового аналога до текстурирования и возьми за основу его авторские слои, маски и стилизацию. Проверь один законченный образец рядом с аналогом перед обработкой набора. Порядок проверки и эталоны описаны в [разделе о текстурах](Docs/ProductionWorkflow.md#текстуры-и-материалы).
