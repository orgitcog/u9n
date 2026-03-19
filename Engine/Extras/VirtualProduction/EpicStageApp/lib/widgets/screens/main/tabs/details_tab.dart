// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:collection';

import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../../../models/actor_data/light_card_actor_data.dart';
import '../../../../models/api_version.dart';
import '../../../../models/engine_connection.dart';
import '../../../../models/property_modify_operations.dart';
import '../../../../models/settings/details_tab_settings.dart';
import '../../../../models/settings/main_screen_settings.dart';
import '../../../../models/settings/selected_actor_settings.dart';
import '../../../../models/unreal_actor_manager.dart';
import '../../../../models/unreal_transaction_manager.dart';
import '../../../../models/unreal_types.dart';
import '../../../../utilities/constants.dart';
import '../../../../utilities/guarded_refresh_state.dart';
import '../../../elements/delta_slider.dart';
import '../../../elements/dropdown_button.dart';
import '../../../elements/dropdown_text.dart';
import '../../../elements/property_divider.dart';
import '../../../elements/reset_mode_button.dart';
import '../../../elements/stepper.dart';
import '../../../elements/unreal_property_builder.dart';
import '../sidebar/outliner_panel.dart';
import 'base_color_tab.dart';

final _log = Logger('DetailsTab');

/// Which type of actor is being shown in the details tab.
enum _DetailsActorType {
  lightCard,
  colorCorrectWindow,
  colorCorrectRegion,
  multiple,
}

/// A tab that lets the user edit basic details of the selected actors.
class DetailsTab extends StatefulWidget {
  const DetailsTab({Key? key}) : super(key: key);

  static const String iconPath = 'packages/epic_common/assets/icons/details.svg';

  static String getTitle(BuildContext context) => AppLocalizations.of(context)!.tabTitleDetails;

  @override
  State<DetailsTab> createState() => _DetailsTabState();
}

class _DetailsTabState extends State<DetailsTab> with GuardedRefreshState {
  late final UnrealActorManager _actorManager;
  late final SelectedActorSettings _selectedActorSettings;
  late final DetailsTabSettings _tabSettings;

  /// Stream subscriptions to user settings.
  final List<StreamSubscription> _settingSubscriptions = [];

  /// Classes for which this tab can modify properties.
  Set<String> get validClasses {
    final Set<String> classes = {lightCardClassName, colorCorrectRegionClassName};
    classes.addAll(colorCorrectWindowClassNames);
    return classes;
  }

  @override
  void initState() {
    super.initState();

    _actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    _actorManager.watchClassName(lightCardClassName, _onActorUpdate);

    _selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false);
    _settingSubscriptions.addAll([
      _selectedActorSettings.selectedActors.listen(refreshOnData),
    ]);

    _tabSettings = DetailsTabSettings(PreferencesBundle.of(context));
  }

  @override
  void dispose() {
    _actorManager.stopWatchingClassName(lightCardClassName, _onActorUpdate);

    for (final StreamSubscription subscription in _settingSubscriptions) {
      subscription.cancel();
    }

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final _DetailsActorType? actorType = _getActorType();

    return Padding(
      padding: EdgeInsets.all(UnrealTheme.cardMargin),
      child: Row(
        children: [
          Expanded(
            child: Card(
              child: Column(children: [
                CardLargeHeader(
                  title: _getTitle(),
                  subtitle: DetailsTab.getTitle(context),
                  iconPath: _getIconPath(),
                  trailing: const ResetModeButton(),
                ),
                Expanded(child: _createInnerContents(actorType)),
              ]),
            ),
          ),
          PreferenceBuilder(
              preference: Provider.of<MainScreenSettings>(context, listen: false).bIsOutlinerPanelOpen,
              builder: (context, final bool bIsOutlinerPanelOpen) {
                return Row(children: [
                  if (bIsOutlinerPanelOpen) const SizedBox(width: UnrealTheme.cardMargin),
                  if (bIsOutlinerPanelOpen) OutlinerPanel(),
                ]);
              }),
        ],
      ),
    );
  }

  /// Get the title to display at the top of the tab.
  String? _getTitle() {
    final Set<String> selectedActors = _selectedActorSettings.selectedActors.getValue();

    if (selectedActors.length == 1) {
      final UnrealObject? actor = _actorManager.getActorAtPath(selectedActors.first);

      if (actor != null) {
        return actor.name;
      }
    } else if (selectedActors.length > 1) {
      return AppLocalizations.of(context)!.detailsTabMultipleActorsTitle;
    }

    return null;
  }

  /// Get the path for the icon to display at the top of the tab.
  String? _getIconPath() {
    String? iconPath;

    if (_selectedActorSettings.selectedActors.getValue().length == 1) {
      final UnrealObject? actor = _actorManager.getActorAtPath(_selectedActorSettings.selectedActors.getValue().first);

      if (actor != null) {
        iconPath = actor.getIconPath();
      }
    }

    if (iconPath != null) {
      return iconPath;
    }

    return 'packages/epic_common/assets/icons/details.svg';
  }

  /// Called when an actor we're editing has an update from the actor manager.
  void _onActorUpdate(ActorUpdateDetails details) {
    if (mounted && (details.renamedActors.isNotEmpty || details.addedActors.isNotEmpty)) {
      // Force redraw in case we need to update the name/just got the name for an actor we were awaiting
      setState(() {});
    }
  }

  /// Determine the type of actors we're editing.
  _DetailsActorType? _getActorType() {
    _DetailsActorType? actorType;

    final Set<String> selectedActors = _selectedActorSettings.selectedActors.getValue();
    for (final String actorPath in selectedActors) {
      final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);
      if (actor == null) {
        continue;
      }

      _DetailsActorType? newActorType;

      if (actor.isA(lightCardClassName)) {
        newActorType = _DetailsActorType.lightCard;
      }

      if (actor.isA(colorCorrectRegionClassName)) {
        newActorType = _DetailsActorType.colorCorrectRegion;
      }

      if (actor.isAny(colorCorrectWindowClassNames)) {
        newActorType = _DetailsActorType.colorCorrectWindow;
      }

      if (actorType != null && newActorType != null && newActorType != actorType) {
        return _DetailsActorType.multiple;
      }

      actorType = newActorType;
    }

    return actorType;
  }

  /// Create the inner contents of the tab (i.e. inside the panel UI).
  Widget _createInnerContents(_DetailsActorType? actorType) {
    if (actorType == null) {
      final mainScreenSettings = Provider.of<MainScreenSettings>(context, listen: false);
      return PreferenceBuilder(
        preference: mainScreenSettings.bIsOutlinerPanelOpen,
        builder: (context, final bool bIsOutlinerPanelOpen) => EmptyPlaceholder(
          message: AppLocalizations.of(context)!.detailsTabEmptyMessage,
          button: bIsOutlinerPanelOpen
              ? null
              : EpicWideButton(
                  text: AppLocalizations.of(context)!.detailsTabShowOutlinerButtonLabel,
                  iconPath: 'packages/epic_common/assets/icons/outliner.svg',
                  onPressed: () => mainScreenSettings.bIsOutlinerPanelOpen.setValue(true),
                ),
        ),
      );
    }

    if (actorType == _DetailsActorType.multiple) {
      return EmptyPlaceholder(
        message: AppLocalizations.of(context)!.detailsTabMixedActorsMessage,
      );
    }

    late final BaseColorTabMode mode;
    late final List<UnrealProperty> colorProperties;
    late final Widget otherPropertiesColumn;

    String orientationLabel = AppLocalizations.of(context)!.colorTabOrientationPropertiesLabel;

    switch (actorType) {
      case _DetailsActorType.lightCard:
        mode = BaseColorTabMode.color;
        colorProperties = _getPropertiesOnValidActors('Color');
        otherPropertiesColumn = _createLightCardPropertiesColumn();
        break;

      // We handle the OR case here because all CCWs are also CCRs, so they may be flagged as both
      case _DetailsActorType.colorCorrectWindow:
        mode = BaseColorTabMode.colorGrading;
        colorProperties = _getPropertiesOnValidActors('ColorGradingSettings');
        otherPropertiesColumn = _createColorCorrectWindowPropertiesColumn();
        break;

      case _DetailsActorType.colorCorrectRegion:
        mode = BaseColorTabMode.colorGrading;
        colorProperties = _getPropertiesOnValidActors('ColorGradingSettings');
        otherPropertiesColumn = _createColorCorrectRegionPropertiesColumn();

        // CCRs show Transform section instead
        orientationLabel = AppLocalizations.of(context)!.colorTabTransformPropertiesLabel;
        break;

      default:
        throw 'Invalid actor type for top widget in details panel';
    }

    return BaseColorTab(
      colorProperties: colorProperties,
      mode: mode,
      rightSideHeader: Center(
        child: PreferenceBuilder(
          preference: _tabSettings.detailsPropertyDisplayType,
          builder: (context, DetailsPropertyDisplayType detailsPropertyDisplayType) => SelectorBar(
            value: detailsPropertyDisplayType,
            onSelected: (DetailsPropertyDisplayType value) => _tabSettings.detailsPropertyDisplayType.setValue(value),
            valueNames: {
              DetailsPropertyDisplayType.orientation: orientationLabel,
              DetailsPropertyDisplayType.appearance: AppLocalizations.of(context)!.colorTabAppearancePropertiesLabel,
            },
          ),
        ),
      ),
      rightSideContents: otherPropertiesColumn,
    );
  }

  /// Create property widgets for a lightcard.
  Widget _createLightCardPropertiesColumn() {
    final engineConnection = Provider.of<EngineConnectionManager>(context, listen: false);
    final EpicStageAppAPIVersion? apiVersion = engineConnection.apiVersion;

    return PreferenceBuilder(
      preference: _tabSettings.detailsPropertyDisplayType,
      builder: (context, DetailsPropertyDisplayType detailsPropertyDisplayType) {
        late List<Widget> widgets;

        switch (detailsPropertyDisplayType) {
          case DetailsPropertyDisplayType.orientation:
            widgets = [
              UnrealDropdownSelector(
                overrideName: AppLocalizations.of(context)!.propertyLightCardMask,
                unrealProperties: _getPropertiesOnValidActors('Mask'),
              ),
              for (final Widget slider in _createCommonStageActorOrientationPropertyWidgets()) slider,
              const PropertyDivider(),
              if (apiVersion?.bIsLightCardBlendingModeAvailable == true)
                UnrealDropdownSelector(
                  overrideName: AppLocalizations.of(context)!.propertyLightCardBlendingMode,
                  unrealProperties: _getPropertiesOnValidActors('PerLightcardRenderMode'),
                ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyLightCardSortPriority,
                bIsInteger: true,
                softMin: 0,
                softMax: 5,
                unrealProperties: _getPropertiesOnValidActors(
                  'TranslucencySortPriority',
                  subObjectName: 'LightCard',
                ),
              ),
            ];
            break;

          case DetailsPropertyDisplayType.appearance:
            widgets = [
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Temperature'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Tint'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Gain'),
                softMin: 0,
                softMax: 10,
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Opacity'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Feathering'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Exposure'),
                softMin: -10,
                softMax: 10,
              ),
              UnrealStepper(
                unrealProperties: _getPropertiesOnValidActors('Exposure'),
                steps: StepperStepConfig.exposureSteps,
              ),
            ];
            break;

          default:
            widgets = [];
        }

        return Column(children: widgets);
      },
    );
  }

  /// Create property widgets for a CCR.
  Widget _createColorCorrectRegionPropertiesColumn() {
    return PreferenceBuilder(
      preference: _tabSettings.detailsPropertyDisplayType,
      builder: (context, DetailsPropertyDisplayType detailsPropertyDisplayType) {
        late List<Widget> widgets;

        switch (detailsPropertyDisplayType) {
          case DetailsPropertyDisplayType.appearance:
            final temperatureTypeProperties = _getPropertiesOnValidActors('TemperatureType');

            widgets = [
              UnrealDropdownSelector(
                overrideName: AppLocalizations.of(context)!.propertyCCRType,
                unrealProperties: _getPropertiesOnValidActors('Type'),
              ),
              UnrealMultiPropertyBuilder<String>(
                properties: temperatureTypeProperties,
                fallbackValue: AppLocalizations.of(context)!.mismatchedValuesLabel,
                builder: (_, String? sharedValue, __) => UnrealDeltaSlider(
                  overrideName: sharedValue,
                  unrealProperties: _getPropertiesOnValidActors('Temperature'),
                  buildLabel: (name) => UnrealDropdownText(unrealProperties: temperatureTypeProperties),
                ),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Tint'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Intensity'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Inner'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Outer'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Falloff'),
                hardMax: 1,
              ),
            ];
            break;

          case DetailsPropertyDisplayType.orientation:
            // CCR transform settings live on the "Root" SceneComponent instead of the actor itself
            getPropertiesOnValidActorRoots(String name) => _getPropertiesOnValidActors(
                  name,
                  subObjectName: 'Root',
                );

            widgets = [
              UnrealMultiPropertyBuilder<bool?>(
                properties: [
                  ...getPropertiesOnValidActorRoots('bAbsoluteLocation'),
                  ...getPropertiesOnValidActorRoots('bAbsoluteRotation'),
                  ...getPropertiesOnValidActorRoots('bAbsoluteScale'),
                ],
                fallbackValue: null,
                builder: (context, sharedValue, _) => DropdownSelectorPropertyWrapper(
                  propertyLabel: AppLocalizations.of(context)!.propertyTransformMode,
                  child: DropdownSelector<bool?>(
                    value: sharedValue,
                    items: [false, true],
                    hint: AppLocalizations.of(context)!.mismatchedValuesLabel,
                    onChanged: (newValue) => _setTransformModeOnValidActors(newValue ?? false),
                    makeItemName: (item) => switch (item) {
                      true => AppLocalizations.of(context)!.transformModeAbsolute,
                      false => AppLocalizations.of(context)!.transformModeRelative,
                      null => AppLocalizations.of(context)!.mismatchedValuesLabel,
                    },
                  ),
                ),
              ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyLocationX,
                softMin: -100,
                softMax: 100,
                bAreMinMaxRelative: true,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeLocation.X'),
              ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyLocationY,
                softMin: -100,
                softMax: 100,
                bAreMinMaxRelative: true,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeLocation.Y'),
              ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyLocationZ,
                softMin: -100,
                softMax: 100,
                bAreMinMaxRelative: true,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeLocation.Z'),
              ),
              const PropertyDivider(padding: 14),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyRotationPitch,
                softMin: -180,
                softMax: 180,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeRotation.Pitch'),
              ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyRotationYaw,
                softMin: -180,
                softMax: 180,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeRotation.Yaw'),
              ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyRotationRoll,
                softMin: -180,
                softMax: 180,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeRotation.Roll'),
              ),
              const PropertyDivider(padding: 14),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyScaleX,
                softMin: 0,
                softMax: 10,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeScale3D.X'),
              ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyScaleY,
                softMin: 0,
                softMax: 10,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeScale3D.Y'),
              ),
              UnrealDeltaSlider(
                overrideName: AppLocalizations.of(context)!.propertyScaleZ,
                softMin: 0,
                softMax: 10,
                unrealProperties: getPropertiesOnValidActorRoots('RelativeScale3D.Z'),
              ),
            ];
            break;
        }

        return Column(children: widgets);
      },
    );
  }

  /// Create property widgets for a CCW.
  Widget _createColorCorrectWindowPropertiesColumn() {
    return PreferenceBuilder(
      preference: _tabSettings.detailsPropertyDisplayType,
      builder: (context, DetailsPropertyDisplayType detailsPropertyDisplayType) {
        late List<Widget> widgets;

        switch (detailsPropertyDisplayType) {
          case DetailsPropertyDisplayType.orientation:
            widgets = [
              UnrealDropdownSelector(
                overrideName: AppLocalizations.of(context)!.propertyLightCardMask,
                unrealProperties: _getPropertiesOnValidActors('WindowType'),
              ),
              for (final Widget slider in _createCommonStageActorOrientationPropertyWidgets()) slider,
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors(
                  'RadialOffset',
                  modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
                ),
              ),
            ];
            break;

          default:
            return _createColorCorrectRegionPropertiesColumn();
        }

        return Column(children: widgets);
      },
    );
  }

  /// Create property widgets for controlling the selected actors' orientations.
  List<Widget> _createCommonStageActorOrientationPropertyWidgets() {
    return _createScalePropertyWidgets() +
        _createPositionPropertyWidgets() +
        [
          UnrealDeltaSlider(
            key: Key('Spin'),
            unrealProperties: _getPropertiesOnValidActors(
              'Spin',
              modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
            ),
            minMaxBehaviour: PropertyMinMaxBehaviour.loop,
          ),
        ];
  }

  /// Create sliders for controlling the position of the selected actors.
  List<Widget> _createPositionPropertyWidgets() {
    // Classes for actor that can be positioned
    final List<String> positionedActorClasses = [lightCardClassName];
    positionedActorClasses.addAll(colorCorrectWindowClassNames);

    // Determine which actors are UV/non-UV so we know which position sliders to show.
    final List<String> uvActorPaths = [];
    final List<String> nonUVActorPaths = [];

    for (final String actorPath in _selectedActorSettings.selectedActors.getValue()) {
      final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);
      if (!(actor?.isAny(positionedActorClasses) ?? false)) {
        // Actor is not a class with position properties, so leave it out entirely
        continue;
      }

      final LightCardActorData? lightCardActorData = actor!.getPerClassData<LightCardActorData>();
      if (lightCardActorData?.bIsUV == true) {
        uvActorPaths.add(actorPath);
      } else {
        nonUVActorPaths.add(actorPath);
      }
    }

    final List<Widget> positionSliders = [];
    if (nonUVActorPaths.isNotEmpty) {
      positionSliders.addAll([
        UnrealDeltaSlider(
          key: Key('Latitude'),
          unrealProperties: _getPropertiesOnActors(
            'Latitude',
            nonUVActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          minMaxBehaviour: PropertyMinMaxBehaviour.clamp,
        ),
        UnrealDeltaSlider(
          key: Key('Longitude'),
          unrealProperties: _getPropertiesOnActors(
            'Longitude',
            nonUVActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          minMaxBehaviour: PropertyMinMaxBehaviour.loop,
        ),
      ]);
    } else if (uvActorPaths.isNotEmpty) {
      positionSliders.addAll([
        UnrealDeltaSlider(
          key: Key('UV X'),
          unrealProperties: _getPropertiesOnActors('UVCoordinates.X', uvActorPaths),
          overrideName: AppLocalizations.of(context)!.propertyLightCardUVX,
          minMaxBehaviour: PropertyMinMaxBehaviour.clamp,
        ),
        UnrealDeltaSlider(
          key: Key('UV Y'),
          unrealProperties: _getPropertiesOnActors('UVCoordinates.Y', uvActorPaths),
          overrideName: AppLocalizations.of(context)!.propertyLightCardUVY,
          minMaxBehaviour: PropertyMinMaxBehaviour.clamp,
        ),
      ]);
    }

    return positionSliders;
  }

  /// Create sliders for controlling the scale of the selected actors.
  List<Widget> _createScalePropertyWidgets() {
    // Get actors that can be scaled
    final List<String> scalableActorClasses = [lightCardClassName];
    scalableActorClasses.addAll(colorCorrectWindowClassNames);

    final Iterable<String> scalableActorPaths = _getValidActorPathsOfClasses(scalableActorClasses);

    final bool bHasNonUvLightCards = scalableActorPaths.any(
      (actorPath) => _actorManager.getActorAtPath(actorPath)?.getPerClassData<LightCardActorData>()?.bIsUV != true,
    );

    final List<Widget> scaleSliders = [];
    if (scalableActorPaths.isNotEmpty) {
      scaleSliders.addAll([
        UnrealDeltaSlider(
          key: Key('Scale X'),
          unrealProperties: _getPropertiesOnActors(
            'Scale.X',
            scalableActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          overrideName: AppLocalizations.of(context)!.propertyLightCardScaleX,
          hardMin: 0,
          softMax: bHasNonUvLightCards ? 10 : 1,
        ),
        UnrealDeltaSlider(
          key: Key('Scale Y'),
          unrealProperties: _getPropertiesOnActors(
            'Scale.Y',
            scalableActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          overrideName: AppLocalizations.of(context)!.propertyLightCardScaleY,
          hardMin: 0,
          softMax: bHasNonUvLightCards ? 10 : 1,
        ),
      ]);
    }

    return scaleSliders;
  }

  /// Given an [actorPath] and a [propertyName], return a modified positional property name accounting for the actor's
  /// type.
  String _modifyPositionalPropertyNameBasedOnClass(String actorPath, String propertyName) {
    final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);

    if (actor != null && actor.isAny(colorCorrectWindowClassNames)) {
      // CCWs store their positional properties in a sub-structure.
      return 'PositionalParams.' + propertyName;
    }

    return propertyName;
  }

  /// Get a list of properties with the given [name] for all of the actors with paths in [actorPaths].
  /// If [modifierFunction] is provided, it will be called for each [actorPath] and the [propertyName] of the property,
  /// and its return value will be used in place of [name].
  List<UnrealProperty> _getPropertiesOnActors(
    String name,
    Iterable<String> actorPaths, {
    String Function(String actorPath, String propertyName)? modifierFunction,
    String? typeNameOverride,
  }) {
    return actorPaths
        .map(
          (actorPath) => UnrealProperty(
            objectPath: actorPath,
            propertyName: (modifierFunction != null) ? modifierFunction(actorPath, name) : name,
            typeNameOverride: typeNameOverride,
          ),
        )
        .toList();
  }

  /// Get a list of properties with the given [name] for all selected actors that belong to a valid class.
  /// If [modifierFunction] is provided, it will be called for each [actorPath] and the [propertyName] of the property,
  /// and its return value will be used in place of [name].
  /// If [typeNameOverride] is provided, it will be used instead of the default typename for the property.
  /// If [subObjectName] is provided, it it will be appended to actor paths.
  List<UnrealProperty> _getPropertiesOnValidActors(
    String name, {
    String Function(String actorPath, String propertyName)? modifierFunction,
    String? typeNameOverride,
    String? subObjectName,
  }) {
    Iterable<String> paths = _getValidActorPaths();

    if (subObjectName != null) {
      paths = paths.map((path) => path + '.' + subObjectName);
    }

    return _getPropertiesOnActors(
      name,
      paths,
      modifierFunction: modifierFunction,
      typeNameOverride: typeNameOverride,
    );
  }

  /// Check if an actor is in the set of valid classes.
  bool _isActorOfValidClass(String actorPath) {
    final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);
    if (actor == null) {
      return false;
    }

    final UnmodifiableSetView<String> actorClasses = actor.classNames;
    return validClasses.any((className) => actorClasses.contains(className));
  }

  /// Get a list of selected actor paths that we want to edit (i.e. have valid classes).
  Iterable<String> _getValidActorPaths() {
    return _selectedActorSettings.selectedActors
        .getValue()
        .where((actorPath) => _isActorOfValidClass(actorPath))
        .toList();
  }

  /// Return all valid selected actors actors that are a member of at least one class in [classNames].
  Iterable<String> _getValidActorPathsOfClasses(List<String> classNames) => _getValidActorPaths()
      .where((path) => _actorManager.getActorAtPath(path)?.isAny(classNames) ?? false)
      .toList(growable: false);

  /// Set the transform modes of all valid actors' root SceneComponents. If [bNewIsAbsolute] is true, they will be set
  /// to Absolute mode; otherwise, they will be set to Relative.
  void _setTransformModeOnValidActors(bool bNewIsAbsolute) async {
    final transactionManager = Provider.of<UnrealTransactionManager>(context, listen: false);

    // Start a transaction
    if (!transactionManager.beginTransaction(AppLocalizations.of(context)!.transactionChangeTransformModes)) {
      _log.warning('Failed to begin transaction for _setTransformModeOnValidActors');
      return;
    }

    // Helper function to create a function call HTTP request
    makeFunctionCall(String objectPath, String functionName, [bool generateTransaction = false, parameters = null]) =>
        UnrealHttpRequest(
          url: '/remote/object/call',
          verb: 'PUT',
          body: {
            'objectPath': objectPath,
            'functionName': functionName,
            'parameters': parameters ?? {},
            'generateTransaction': generateTransaction,
          },
        );

    // Get the current world transform of each component
    final componentPaths = _getValidActorPaths().map((path) => path + '.Root').toList(growable: false);
    final worldTransformRequests = componentPaths.map((componentPath) => makeFunctionCall(
          componentPath,
          'GetWorldTransform',
        ));

    final connectionManager = Provider.of<EngineConnectionManager>(context, listen: false);
    final UnrealHttpResponse worldTransformResponse =
        await connectionManager.sendBatchedHttpRequest(worldTransformRequests);

    if (worldTransformResponse.code != HttpResponseCode.ok) {
      _log.warning('Failed to get world transforms for components in _setTransformModeOnValidActors');
      transactionManager.endTransaction();
      return;
    }

    if (worldTransformResponse.body.length != componentPaths.length) {
      _log.warning('Wrong number of component world transforms received in _setTransformModeOnValidActors');
      transactionManager.endTransaction();
      return;
    }

    // Successfully received transforms. Now update each component's transform modes, then set world transform back
    // so that the coordinates are correct in the new modes.
    final List<UnrealHttpRequest> updateTransformRequests = [];

    for (int componentIndex = 0; componentIndex < componentPaths.length; ++componentIndex) {
      final String componentPath = componentPaths[componentIndex];
      final worldTransform = worldTransformResponse.body?[componentIndex]?.body?['ReturnValue'];

      if (worldTransform == null) {
        _log.warning('No world transform received for $componentPath');
        continue;
      }

      updateTransformRequests.add(makeFunctionCall(componentPath, 'SetAbsolute', true, {
        'bNewAbsoluteLocation': bNewIsAbsolute,
        'bNewAbsoluteRotation': bNewIsAbsolute,
        'bNewAbsoluteScale': bNewIsAbsolute,
      }));

      updateTransformRequests.add(
        makeFunctionCall(componentPath, 'SetWorldTransform', true, {
          'NewTransform': worldTransform,
        }),
      );
    }

    connectionManager.sendBatchedHttpRequest(updateTransformRequests);

    // Finally, close out the transaction
    transactionManager.endTransaction();
  }
}
