// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import 'delta_widget_base.dart';

/// A divider used to split up groups of property widgets.
class PropertyDivider extends StatelessWidget {
  const PropertyDivider({super.key, this.padding = 16});

  /// Padding under the divider.
  final double padding;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(
        left: DeltaWidgetConstants.widgetOuterXPadding,
        right: DeltaWidgetConstants.widgetOuterXPadding,
        bottom: padding,
      ),
      child: Container(
        color: Theme.of(context).colorScheme.background,
        height: 3,
      ),
    );
  }
}
