// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../models/property_modify_operations.dart';
import '../../models/settings/delta_widget_settings.dart';
import 'delta_widget_base.dart';
import 'unreal_widget_base.dart';

/// A dropdown that controls one or more enum properties in Unreal Engine.
class UnrealDropdownSelector extends UnrealWidget {
  const UnrealDropdownSelector({
    super.key,
    super.overrideName,
    super.enableProperties,
    required super.unrealProperties,
  });

  @override
  _UnrealDropdownButtonState createState() => _UnrealDropdownButtonState();
}

class _UnrealDropdownButtonState extends _UnrealDropdownSelectorOuterState<UnrealDropdownSelector, String> {
  @override
  void validateValue(dynamic value) {
    assert(value is String);
    assert(propertyEnumValues.contains(value));
  }

  @override
  Widget buildDropdownSelector({
    required bool bIsInResetMode,
    required SingleSharedValue<String> sharedValue,
    required String hint,
  }) =>
      DropdownSelector<String>(
        items: propertyEnumValues,
        value: sharedValue.value,
        onChanged: onChanged,
        hint: hint,
        bIsEnabled: !bIsInResetMode,
      );
}

/// A dropdown that controls one or more boolean properties in Unreal Engine.
class UnrealBooleanDropdownSelector extends UnrealWidget {
  const UnrealBooleanDropdownSelector({
    super.key,
    super.overrideName,
    super.enableProperties,
    required super.unrealProperties,
  });

  @override
  _UnrealBooleanDropdownSelectorState createState() => _UnrealBooleanDropdownSelectorState();
}

class _UnrealBooleanDropdownSelectorState
    extends _UnrealDropdownSelectorOuterState<UnrealBooleanDropdownSelector, bool> {
  @override
  void validateValue(dynamic value) {
    assert(value is bool);
  }

  @override
  Widget buildDropdownSelector({
    required bool bIsInResetMode,
    required SingleSharedValue<bool> sharedValue,
    required String hint,
  }) =>
      DropdownSelector<bool>(
        items: [false, true],
        makeItemName: (item) => switch (item) {
          true => AppLocalizations.of(context)!.dropdownSelectorEnabledLabel,
          false => AppLocalizations.of(context)!.dropdownSelectorDisabledLabel,
        },
        value: sharedValue.value,
        onChanged: onChanged,
        hint: hint,
        bIsEnabled: !bIsInResetMode,
      );
}

abstract class _UnrealDropdownSelectorOuterState<WidgetType extends UnrealWidget, PropertyType>
    extends State<WidgetType> with UnrealWidgetStateMixin<WidgetType, PropertyType> {
  @override
  PropertyModifyOperation get modifyOperation => const SetOperation();

  @override
  Widget build(BuildContext context) {
    final SingleSharedValue<PropertyType> sharedValue = getSingleSharedValue();

    return TransientPreferenceBuilder(
      preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
      builder: (context, final bool bIsInResetMode) => DropdownSelectorPropertyWrapper(
        propertyLabel: propertyLabel,
        bIsInResetMode: bIsInResetMode,
        onReset: handleOnResetByUser,
        child: buildDropdownSelector(
          bIsInResetMode: bIsInResetMode,
          sharedValue: sharedValue,
          hint: sharedValue.bHasMultipleValues ? AppLocalizations.of(context)!.mismatchedValuesLabel : '',
        ),
      ),
    );
  }

  /// Validates the user-provided value before it's set.
  @protected
  void validateValue(dynamic value) {}

  /// Build the inner dropdown selector, taking into account whether the widget [bIsInResetMode], the current
  /// [sharedValue] between all multi-selected objects, and the [hint] to display if the shared value is null.
  Widget buildDropdownSelector({
    required bool bIsInResetMode,
    required SingleSharedValue<PropertyType> sharedValue,
    required String hint,
  });

  /// Called when the user changes the value using the dropdown.
  @protected
  void onChanged(dynamic newValue) {
    validateValue(newValue);

    handleOnChangedByUser(List.filled(widget.unrealProperties.length, newValue));
    endTransaction();
  }
}

/// Wrapper for a [DropdownSelector] that styles it as an Unreal property.
/// This only applies styling; see UnrealDropdownSelector for an implementation that actually talks to the engine.
class DropdownSelectorPropertyWrapper extends StatelessWidget {
  const DropdownSelectorPropertyWrapper({
    super.key,
    required this.child,
    required this.propertyLabel,
    this.bIsInResetMode = false,
    this.onReset,
  });

  /// The DropdownSelector to display inside the wrapper.
  final Widget child;

  /// The label of the property to show to users.
  final String propertyLabel;

  /// If true, show a reset button next to the widget.
  final bool bIsInResetMode;

  /// Callback function for when the reset button is pressed.
  final Function()? onReset;

  @override
  Widget build(BuildContext context) {
    final TextStyle textStyle = Theme.of(context).textTheme.labelMedium!;

    return Padding(
      padding: const EdgeInsets.only(bottom: 16),
      child: Row(
        children: [
          Expanded(
            child: Padding(
              padding: EdgeInsets.symmetric(horizontal: DeltaWidgetConstants.widgetOuterXPadding),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.start,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    propertyLabel,
                    style: textStyle.copyWith(
                      color: textStyle.color!.withOpacity(bIsInResetMode ? 0.4 : 1.0),
                    ),
                  ),
                  Padding(
                    padding: const EdgeInsets.only(
                      left: DeltaWidgetConstants.widgetInnerXPadding,
                      right: DeltaWidgetConstants.widgetInnerXPadding,
                      top: 4,
                    ),
                    child: child,
                  ),
                ],
              ),
            ),
          ),
          if (bIsInResetMode)
            Padding(
              padding: const EdgeInsets.only(right: DeltaWidgetConstants.widgetOuterXPadding),
              child: ResetValueButton(onPressed: onReset),
            ),
        ],
      ),
    );
  }
}
