// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';

import '../../../../../models/navigator_keys.dart';
import 'pages/settings_advanced_view.dart';
import 'pages/settings_compression_picker.dart';
import 'pages/settings_dialog_main.dart';
import 'pages/settings_log_list.dart';
import 'pages/settings_log_view.dart';
import 'pages/settings_root_actor_picker.dart';

/// Dialog containing the app's settings menu.
class SettingsDialog extends StatefulWidget {
  const SettingsDialog({
    Key? key,
    this.initialRoute,
  }) : super(key: key);

  /// The initial route to show.
  final String? initialRoute;

  /// Show the settings dialog.
  /// If [initialRoute] is provided, open to that route instead of the main settings page.
  static void show([String? initialRoute]) {
    final route = GenericModalDialogRoute(
      builder: (_) => SettingsDialog(
        initialRoute: initialRoute,
      ),
    );

    Navigator.of(rootNavigatorKey.currentContext!, rootNavigator: true).push(route);
  }

  @override
  State<StatefulWidget> createState() => _SettingsDialogState();
}

class _SettingsDialogState extends State<SettingsDialog> with RouteAware {
  /// Key corresponding to the inner navigator of the settings menu.
  final _innerNavigatorKey = GlobalKey<NavigatorState>();

  @override
  Widget build(BuildContext context) {
    return ModalDialogCard(
      color: Theme.of(context).colorScheme.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
      ),
      child: ConstrainedBox(
        constraints: BoxConstraints(
          maxWidth: 600,
          minHeight: 250,
        ),
        child: IntrinsicHeight(
          child: Navigator(
            key: _innerNavigatorKey,
            initialRoute: widget.initialRoute,
            onGenerateRoute: (final RouteSettings settings) {
              late final Widget page;

              switch (settings.name) {
                case SettingsDialogMain.route:
                  page = const SettingsDialogMain();
                  break;

                case SettingsDialogRootActorPicker.route:
                  page = const SettingsDialogRootActorPicker();
                  break;

                case SettingsLogList.route:
                  page = const SettingsLogList();
                  break;

                case SettingsLogView.route:
                  page = const SettingsLogView();
                  break;

                case SettingsAdvancedView.route:
                  page = const SettingsAdvancedView();
                  break;

                case SettingsCompressionPicker.route:
                  page = const SettingsCompressionPicker();
                  break;

                default:
                  throw Exception('No settings route named ${settings.name}');
              }

              return PageRouteBuilder(
                settings: settings,
                transitionDuration: Duration.zero,
                reverseTransitionDuration: Duration.zero,
                pageBuilder: (_, __, ___) => page,
              );
            },
          ),
        ),
      ),
    );
  }
}
