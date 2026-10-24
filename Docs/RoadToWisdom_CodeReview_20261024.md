# RoadToWisdom — C++ code review / следующий шаг (24.10.2026)

**Основание:** `project-code-dump-001(20261024-124213).txt`, только `Source/RoadToWisdom`; не включает содержимое `Content`, Editor maps и исходники UT plugins. Выводы о runtime нуждаются в проверке.

## Приоритет P0: блокирующие риски

1. **Неправильное использование ConstructorHelpers:** `Gameplay/Character/RoadToWisdomOnFootCharacter.cpp`, `SetupPlayerInputComponent()` конструирует `FObjectFinder<UInputAction>` вне конструктора. По контракту Unreal это недопустимо. Перенести разрешение asset references в конструктор/UPROPERTY; binding оставить в SetupPlayerInputComponent.
2. **Нет видимой инициализации `VanPawn` при стартовом on-foot possession:** `Gameplay/RoadToWisdomExplorerController.cpp` заполняет VanPawn только если possesed vehicle, а `Gameplay/RoadToWisdomQuietRoadGameMode.cpp` начинает пешком. Если карта не решает проблему, взаимодействие с фургоном невозможно. Не вводить дополнительные поиски каждый Tick: однократная регистрация и проверка ровно одного активного vehicle.
3. **Restore может не примениться из-за порядка событий:** `Gameplay/Missions/RoadToWisdomSaveCoordinator.cpp::TryRestoreSession()` подготавливает `bHasPendingRestore`, а `Gameplay/RoadToWisdomExplorerController.cpp::OnPossess()` единственный раз планирует `ApplyPendingSessionRestore()`. При уже случившемся OnPossess новое pending может остаться невыполненным. Развязать инициализацию и применение.

## P1: корректность и регрессии

4. **Сохранение только на advance миссии:** `MissionManager::AdvanceStage -> SaveSession`. Позиция после завершённой миссии не гарантируется. Добавить одно осмысленное ручное действие через радио/панель фургона после стабилизации.
5. **Результаты WriteSection и WorldCapture:** в `CaptureAndWriteSections()` не проверяется успешность всех `WriteSection`, а при failure CaptureWorldState вызывается дальнейший SaveToSlot. Проверить контракт UTSave, не сохранять частичный snapshot молча.
6. **Проверка выхода:** `IsExitSpotClear()` использует сферу `SphereTraceSingle` по `ECC_Pawn`, а не полный объём капсулы. Проверить на земле/стене/дереве/перепаде.
7. **Посадка у центра машины:** `TryEnterVehicle()` оценивает расстояние до `VanPawn->GetActorLocation()` (центра), а не водительской двери. С ростом кузова camper van это даст неинтуитивные случаи.
8. **Гарантия UI state при BeginPlay:** `MissionManager::SetObjectiveText()` вызывает контроллер, но HUD создаётся в Controller.BeginPlay; надо синхронизировать актуальную objective после создания HUD.
9. **Stable ID:** `ARoadToWisdomFuseBox::SaveStableId` по умолчанию один. Для нескольких объектов нужно обеспечить уникальность и проверять дубликаты в карте.
10. **Механика взаимодействий:** `Interactor` вручную `UpdateFocus()` каждый Tick с отключённым тиком самого компонента; проверить, что удерживаемые interactions/фокус/подсказки по контракту UTInteraction не сломаны. Не добавлять второй независимый trace.
11. **Смешение template и game controller:** `ExplorerController` наследуется от `ARoadToWisdomPlayerController`, но намеренно обходит базовый `SetupInputComponent` и `OnPossess`. Оценить в отдельном небольшом рефакторе, имеет ли смысл прямое наследование от APlayerController — ТОЛЬКО после проверки ссылок Blueprint, либо оставить как есть.
12. **Неиспользуемые variants:** `Variant_TimeTrial`, `SportsCar` и Offroad present в Source. Не удалять, пока не проверены uasset references; позднее можно сократить исходники/контент аккуратно.

## Не проверено

Наличие и правильность самой карты, работоспособность input assets, APIs UT plugins, compile/link, logging, packaged, реальные размеры Content/Git.
