import 'dart:async';
import 'dart:math';
import 'package:flutter/material.dart';
import '../home/main_screen.dart';

class IntroScreen extends StatefulWidget {
  const IntroScreen({super.key});

  @override
  State<IntroScreen> createState() => _IntroScreenState();
}

class _IntroScreenState extends State<IntroScreen> {
  late final String _assetPath;
  static const _images = [
    'assets/images/intro1.png',
    'assets/images/intro2.png',
    'assets/images/intro3.png',
    'assets/images/intro4.png',
    'assets/images/intro5.png',
  ];

  @override
  void initState() {
    super.initState();
    _assetPath = _images[Random().nextInt(_images.length)];
    Timer(
      const Duration(seconds: 1),
      () => Navigator.pushReplacement(
        context,
        MaterialPageRoute(builder: (_) => const MainScreen()),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Stack(
        fit: StackFit.expand,
        children: [
          Image.asset(_assetPath, fit: BoxFit.cover),
          Container(color: Colors.black.withOpacity(0.35)),
          const Center(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  'Oops',
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: 48,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                SizedBox(height: 8),
                Text(
                  'Only one perfect step',
                  style: TextStyle(color: Colors.white70, fontSize: 16),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
