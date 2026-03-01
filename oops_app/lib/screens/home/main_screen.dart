import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:o3d/o3d.dart';

class MainScreen extends StatefulWidget {
  const MainScreen({super.key});

  @override
  State<MainScreen> createState() => _MainScreenState();
}

class _MainScreenState extends State<MainScreen> {
  final _pageController = PageController(viewportFraction: 0.97);
  final _o3dController = O3DController();
  int _currentPage = 0;
  int _tabIndex = 0;

  final _heroCards = const [
    HeroCardData(
      image: 'assets/images/hero1.jpg',
      title: '오늘 뭐 신었어?',
      subtitle: '내 최애 신발 자랑하러 가기',
    ),
    HeroCardData(
      image: 'assets/images/hero2.jpg',
      title: '여러분의 최고의 한 걸음',
      subtitle: '추천 코디 확인하고 경품 응모하기',
    ),
    HeroCardData(
      image: 'assets/images/hero3.jpg',
      title: '마라톤 준비 중',
      subtitle: '러닝 챌린지 참여하기',
    ),
  ];

  @override
  void dispose() {
    _pageController.dispose();
    _o3dController.pause();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFFF3F2F2),
      body: SafeArea(
        child: IndexedStack(
          index: _tabIndex,
          children: [
            _buildHomeTab(),
            _FootMeasurementTab(controller: _o3dController),
            const _TabPlaceholder(label: 'OOTD'),
            const _TabPlaceholder(label: '즐겨찾기'),
            const _TabPlaceholder(label: '프로필'),
          ],
        ),
      ),
      bottomNavigationBar: BottomNavigationBar(
        backgroundColor: Colors.white,
        selectedItemColor: Colors.black,
        unselectedItemColor: Colors.black54,
        currentIndex: _tabIndex,
        onTap: (index) => setState(() => _tabIndex = index),
        type: BottomNavigationBarType.fixed,
        items: const [
          BottomNavigationBarItem(icon: Icon(Icons.straighten), label: '추천'),
          BottomNavigationBarItem(icon: Icon(Icons.pets), label: '발측정'),
          BottomNavigationBarItem(icon: Icon(Icons.style), label: 'OOTD'),
          BottomNavigationBarItem(
            icon: Icon(Icons.favorite_border),
            label: '즐겨찾기',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.person_outline),
            label: '프로필',
          ),
        ],
      ),
    );
  }

  Widget _buildHomeTab() {
    return Column(
      children: [
        const _HomeTopBar(),
        const SizedBox(height: 12),
        _HeroCarousel(
          controller: _pageController,
          cards: _heroCards,
          currentIndex: _currentPage,
          onPageChanged: (index) => setState(() => _currentPage = index),
        ),
        const SizedBox(height: 16),
        Expanded(
          child: ListView(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            children: const [
              SizedBox(height: 24),
              Center(
                child: Text(
                  '추후 섹션을 이 영역에 추가할 예정입니다.',
                  style: TextStyle(fontSize: 16),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _HomeTopBar extends StatelessWidget {
  const _HomeTopBar();

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      child: Row(
        children: [
          const Text(
            'Oops',
            style: TextStyle(fontSize: 24, fontWeight: FontWeight.w600),
          ),
          const Spacer(),
          IconButton(onPressed: () {}, icon: const Icon(Icons.search)),
          IconButton(onPressed: () {}, icon: const Icon(Icons.person_outline)),
        ],
      ),
    );
  }
}

class _HeroCarousel extends StatelessWidget {
  const _HeroCarousel({
    required this.controller,
    required this.cards,
    required this.currentIndex,
    required this.onPageChanged,
  });

  final PageController controller;
  final List<HeroCardData> cards;
  final int currentIndex;
  final ValueChanged<int> onPageChanged;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 320,
      child: Stack(
        alignment: Alignment.bottomCenter,
        children: [
          PageView.builder(
            controller: controller,
            onPageChanged: onPageChanged,
            itemCount: cards.length,
            itemBuilder: (context, index) => _HeroCard(data: cards[index]),
          ),
          Row(
            mainAxisSize: MainAxisSize.min,
            children: List.generate(cards.length, (index) {
              final isActive = index == currentIndex;
              return AnimatedContainer(
                duration: const Duration(milliseconds: 250),
                margin: const EdgeInsets.symmetric(horizontal: 4, vertical: 12),
                width: isActive ? 20 : 8,
                height: 8,
                decoration: BoxDecoration(
                  color: isActive
                      ? Colors.white
                      : Colors.white.withOpacity(0.4),
                  borderRadius: BorderRadius.circular(4),
                ),
              );
            }),
          ),
        ],
      ),
    );
  }
}

class _HeroCard extends StatelessWidget {
  const _HeroCard({required this.data});

  final HeroCardData data;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 8),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(32),
        child: Stack(
          fit: StackFit.expand,
          children: [
            Image.asset(data.image, fit: BoxFit.cover),
            Container(
              decoration: const BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                  colors: [Colors.transparent, Colors.black54],
                ),
              ),
            ),
            Positioned(
              left: 20,
              right: 20,
              bottom: 24,
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    data.title,
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 22,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 4),
                  Text(
                    data.subtitle,
                    style: const TextStyle(color: Colors.white70, fontSize: 14),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class HeroCardData {
  const HeroCardData({
    required this.image,
    required this.title,
    required this.subtitle,
  });

  final String image;
  final String title;
  final String subtitle;
}

class _FootMeasurementTab extends StatefulWidget {
  const _FootMeasurementTab({required this.controller});

  final O3DController controller;

  @override
  State<_FootMeasurementTab> createState() => _FootMeasurementTabState();
}

class _FootMeasurementTabState extends State<_FootMeasurementTab> {
  bool _isScanning = false;
  String? _statusMessage;
  String? _glbUrl;
  Timer? _pollTimer;
  _FootMetrics? _footMetrics;
  final Random _random = Random();

  static const _scannerBase = 'http://70.12.245.109:8000';

  @override
  void dispose() {
    _pollTimer?.cancel();
    super.dispose();
  }

  Future<void> _startScan() async {
    setState(() {
      _isScanning = true;
      _statusMessage = '스캔 요청을 전송 중입니다.';
      _glbUrl = null;
      _footMetrics = null;
    });

    try {
      final response = await http.post(
        Uri.parse('$_scannerBase/scan'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({}),
      );

      if (response.statusCode == 200) {
        final decoded = jsonDecode(response.body) as Map<String, dynamic>;
        final jobId = decoded['job_id'] as String?;
        if (jobId == null) {
          setState(() {
            _statusMessage = '응답에 job_id가 없습니다.';
            _isScanning = false;
          });
        } else {
          setState(() {
            _statusMessage = '스캔 시작! Job ID: $jobId';
          });
          _startPolling(jobId);
        }
      } else {
        setState(() {
          _statusMessage =
              '오류(${response.statusCode}): ${response.body.trim()}';
          _isScanning = false;
        });
      }
    } catch (e) {
      setState(() {
        _statusMessage = '요청 실패: $e';
        _isScanning = false;
      });
    }
  }

  _FootMetrics _generateDummyMetrics() {
    final length = 253 + _random.nextInt(5);
    final width = 95 + _random.nextInt(4);
    final dorsum = 50 + _random.nextInt(8);
    return _FootMetrics(
      lengthMm: length.toDouble(),
      widthMm: width.toDouble(),
      instepHeightMm: dorsum.toDouble(),
    );
  }

  void _startPolling(String jobId) {
    _pollTimer?.cancel();
    _pollTimer = Timer.periodic(
      const Duration(seconds: 3),
      (_) => _fetchJobStatus(jobId),
    );
    _fetchJobStatus(jobId);
  }

  Future<void> _fetchJobStatus(String jobId) async {
    try {
      final response = await http.get(Uri.parse('$_scannerBase/jobs/$jobId'));
      if (response.statusCode != 200) {
        setState(() {
          _statusMessage =
              '상태 조회 실패(${response.statusCode}): ${response.body.trim()}';
        });
        return;
      }
      final payload = jsonDecode(response.body) as Map<String, dynamic>;
      final state = payload['state'] as String?;
      final result = payload['result'] as Map<String, dynamic>?;

      if (state == 'succeeded') {
        final glbUrl = result?['glb_url'] as String?;
        _pollTimer?.cancel();
        if (glbUrl != null) {
          setState(() {
            _glbUrl = glbUrl;
            _statusMessage = '새 GLB가 준비됐어요!';
            _isScanning = false;
            _footMetrics = _generateDummyMetrics();
          });
        } else {
          setState(() {
            _statusMessage = '스캔 성공했지만 GLB URL이 없습니다.';
            _isScanning = false;
          });
        }
      } else if (state == 'failed') {
        _pollTimer?.cancel();
        final error = payload['error'] ?? '원인 불명';
        setState(() {
          _statusMessage = '스캔 실패: $error';
          _isScanning = false;
        });
      } else {
        setState(() {
          final when = payload['updated_at'] ?? '';
          _statusMessage = '진행 중($state) · $when';
        });
      }
    } catch (e) {
      setState(() {
        _statusMessage = '상태 조회 중 오류: $e';
      });
    }
  }

  Widget _buildModelViewer(ValueKey<String> o3dKey, bool hasNetworkModel) {
    if (hasNetworkModel && _glbUrl != null) {
      return O3D.network(
        key: o3dKey,
        controller: widget.controller,
        src: _glbUrl!,
        autoRotate: true,
        autoPlay: true,
        cameraControls: true,
        backgroundColor: const Color(0xFF455D7A),
      );
    }
    return O3D(
      key: o3dKey,
      controller: widget.controller,
      src: 'assets/glbs/test.glb',
      autoRotate: true,
      autoPlay: true,
      cameraControls: true,
      backgroundColor: const Color(0xFF455D7A),
    );
  }

  void _showFootInfoModal() {
    showModalBottomSheet<void>(
      context: context,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
      ),
      builder: (context) {
        final metrics = _footMetrics;
        return Container(
          padding: const EdgeInsets.all(24),
          decoration: const BoxDecoration(
            color: Color(0xFF1D2434),
            borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
          ),
          child: metrics == null
              ? Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: const [
                    Text(
                      '발 정보 준비 중',
                      style: TextStyle(
                        color: Colors.white,
                        fontSize: 20,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                    SizedBox(height: 12),
                    Text(
                      '스캔을 완료하면 발 길이와 너비를 보여드릴게요.',
                      style: TextStyle(color: Colors.white70, fontSize: 16),
                    ),
                    SizedBox(height: 12),
                  ],
                )
              : Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      '발 정보',
                      style: TextStyle(
                        color: Colors.white,
                        fontSize: 20,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                    const SizedBox(height: 20),
                    _buildMetricRow(
                        '발 길이', '${metrics.lengthMm.toStringAsFixed(0)} mm'),
                    const SizedBox(height: 12),
                    _buildMetricRow(
                        '발 너비', '${metrics.widthMm.toStringAsFixed(0)} mm'),
                    const SizedBox(height: 12),
                    _buildMetricRow(
                        '발등 높이', '${metrics.instepHeightMm.toStringAsFixed(0)} mm'),
                    const SizedBox(height: 24),
                    const SizedBox(height: 8),
                  ],
                ),
        );
      },
    );
  }

  Widget _buildMetricRow(String label, String value) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text(
          label,
          style: const TextStyle(color: Colors.white70, fontSize: 16),
        ),
        Text(
          value,
          style: const TextStyle(
            color: Colors.white,
            fontSize: 18,
            fontWeight: FontWeight.w600,
          ),
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final o3dKey = ValueKey(_glbUrl ?? 'default_asset');
    final hasNetworkModel = _glbUrl != null;

    return Column(
      children: [
        const _HomeTopBar(),
        const SizedBox(height: 16),
        Expanded(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Card(
              elevation: 2,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(28),
              ),
              child: ClipRRect(
                borderRadius: BorderRadius.circular(28),
                child: _buildModelViewer(o3dKey, hasNetworkModel),
              ),
            ),
          ),
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          child: Align(
            alignment: Alignment.centerRight,
            child: OutlinedButton.icon(
              onPressed: _footMetrics != null ? _showFootInfoModal : null,
              icon: const Icon(Icons.straighten),
              label: const Text('발 정보 보기'),
            ),
          ),
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16),
          child: SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              onPressed: _isScanning ? null : _startScan,
              icon: _isScanning
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(
                        strokeWidth: 2,
                        color: Colors.white,
                      ),
                    )
                  : const Icon(Icons.pets),
              label: Text(_isScanning ? '스캔 중...' : '족형 측정 시작'),
              style: ElevatedButton.styleFrom(
                padding: const EdgeInsets.symmetric(vertical: 14),
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(16),
                ),
                backgroundColor: Colors.white,
              ),
            ),
          ),
        ),
        if (_statusMessage != null)
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
            child: Text(
              _statusMessage!,
              textAlign: TextAlign.center,
              style: const TextStyle(fontSize: 14),
            ),
          ),
        const Padding(
          padding: EdgeInsets.only(bottom: 24),
          child: Text(
            '모델을 드래그하거나 핀치 제스처로 회전·확대해 발 모양을 확인하세요.',
            textAlign: TextAlign.center,
            style: TextStyle(fontSize: 14),
          ),
        ),
      ],
    );
  }
}

class _TabPlaceholder extends StatelessWidget {
  const _TabPlaceholder({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Text('$label 탭 준비 중입니다.'),
    );
  }
}

class _FootMetrics {
  const _FootMetrics({
    required this.lengthMm,
    required this.widthMm,
    required this.instepHeightMm,
  });

  final double lengthMm;
  final double widthMm;
  final double instepHeightMm;
}
