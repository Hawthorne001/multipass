import 'package:flutter/material.dart';

class ConfirmationDialog extends StatelessWidget {
  final String title;
  final Widget body;
  final String actionText;
  final VoidCallback onAction;
  final String inactionText;
  final VoidCallback onInaction;
  final double width;

  const ConfirmationDialog({
    super.key,
    required this.title,
    required this.body,
    required this.actionText,
    required this.onAction,
    required this.inactionText,
    required this.onInaction,
    this.width = 520,
  });

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      shape: const Border(),
      contentPadding: const EdgeInsets.symmetric(horizontal: 8),
      titlePadding: const EdgeInsets.only(left: 16, right: 8, top: 8),
      buttonPadding: const EdgeInsets.symmetric(horizontal: 16),
      actionsPadding: const EdgeInsets.symmetric(
        horizontal: 16,
      ).copyWith(top: 4, bottom: 12),
      title: Row(
        children: [
          Expanded(child: Text(title)),
          IconButton(
            onPressed: () => Navigator.pop(context),
            splashRadius: 15,
            icon: const Icon(Icons.close),
          ),
        ],
      ),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Divider(),
          SizedBox(
            width: width,
            child: Padding(
              padding: const EdgeInsets.only(top: 8, bottom: 4),
              child: body,
            ),
          ),
          const Divider(),
        ],
      ),
      actions: [
        OutlinedButton(onPressed: onInaction, child: Text(inactionText)),
        TextButton(
          onPressed: onAction,
          style: TextButton.styleFrom(backgroundColor: const Color(0xffC7162B)),
          child: Text(actionText),
        ),
      ],
    );
  }
}
