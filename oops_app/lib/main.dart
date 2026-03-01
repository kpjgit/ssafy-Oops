import 'package:flutter/material.dart';
import 'screens/splash/splash_screen.dart';

void main() {
  runApp(const OopsApp());
}

class OopsApp extends StatelessWidget {
  const OopsApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Oops',
      theme: ThemeData(
        scaffoldBackgroundColor: Colors.black,
        fontFamily: 'Pretendard',
      ),
      home: const SplashScreen(),
    );
  }
}
