// main.dart
/// Press Monitor Application
/// 
/// A professional Flutter application for monitoring industrial press machines
/// via MQTT protocol. Provides real-time status updates and operator interaction
/// capabilities.
/// 
/// Author: Your Team
/// Version: 1.0.0
/// Last Modified: October 2025

import 'dart:async';
import 'dart:convert';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:provider/provider.dart';

// ============================================================================
// CONSTANTS & CONFIGURATION
// ============================================================================

/// Application-wide constants for consistent styling and configuration
class AppConstants {
  // MQTT Configuration
  static const String mqttServer = '192.168.0.52';
  static const int mqttPort = 1883;
  static const String statusTopic = 'alphabase/presses/status';
  static const String commandsTopic = 'alphabase/presses/commands';
  static const int keepAlivePeriod = 30;
  
  // UI Dimensions
  static const double gaugeSize = 250.0;
  static const double gaugeStrokeWidth = 20.0;
  static const double buttonHeight = 60.0;
  static const double defaultPadding = 24.0;
  static const double borderRadius = 12.0;
  
  // Animation
  static const Duration gaugeAnimationDuration = Duration(seconds: 1);
  static const Duration connectionTimeout = Duration(seconds: 10);
}

/// Color palette for consistent theming across the application
class AppColors {
  static const Color primary = Color(0xFF3B82F6);        // blue-500
  static const Color success = Color(0xFF22C55E);        // green-500
  static const Color warning = Color(0xFFF59E0B);        // amber-500
  static const Color danger = Color(0xFFEF4444);         // red-500
  
  static const Color background = Color(0xFFF1F5F9);     // slate-100
  static const Color surface = Colors.white;
  static const Color textPrimary = Color(0xFF1E293B);    // slate-800
  static const Color textSecondary = Color(0xFF334155);  // slate-700
  static const Color textMuted = Color(0xFF475569);      // slate-600
}

/// Text styles for consistent typography
class AppTextStyles {
  static const TextStyle heading = TextStyle(
    fontSize: 24,
    fontWeight: FontWeight.bold,
    color: AppColors.textPrimary,
  );
  
  static const TextStyle subheading = TextStyle(
    fontSize: 18,
    fontWeight: FontWeight.w600,
    color: AppColors.textSecondary,
  );
  
  static const TextStyle body = TextStyle(
    fontSize: 16,
    fontWeight: FontWeight.normal,
    color: AppColors.textSecondary,
  );
  
  static const TextStyle caption = TextStyle(
    fontSize: 14,
    fontWeight: FontWeight.normal,
    color: AppColors.textMuted,
  );
  
  static const TextStyle gaugeValue = TextStyle(
    fontSize: 60,
    fontWeight: FontWeight.bold,
    color: AppColors.textSecondary,
  );
}

// ============================================================================
// MODELS & BUSINESS LOGIC
// ============================================================================

/// Represents the possible states of a press machine
enum PressState { 
  idle,     // Machine is not operating
  running,  // Machine is actively pressing
  waiting   // Machine stopped, waiting for operator input
}

/// Predefined stop reasons for operator selection
/// 
/// TROUBLESHOOTING: If buttons aren't working:
/// 1. Check the debug console when pressing buttons to see what's being sent
/// 2. Check your ESP32 code for the exact strings it expects in the "reason" field
/// 3. Update the exactESP32Strings map below to match what your ESP32 expects
/// 4. Toggle useFullNamesForESP32 between true/false to test different formats
class StopReasons {
  // ⚠️ CONFIGURATION: Set based on what your ESP32 expects
  // true = send full names like "Maintenance Required"
  // false = send simplified keys like "maintenance"
  static const bool useFullNamesForESP32 = true;
  
  // Display labels for the UI
  static const Map<String, String> reasons = {
    'maintenance': 'Maintenance Required',
    'material': 'Material Issue', 
    'tool_change': 'Tool Change',
    'quality': 'Quality Issue',
  };
  
  // ⚠️ UPDATE THIS MAP to match what your ESP32 expects
  // The key is what shows in the UI, the value is what gets sent to ESP32
  static const Map<String, String> exactESP32Strings = {
    'Maintenance Required': 'Maintenance Required',
    'Material Issue': 'Material Issue',
    'Tool Change': 'Tool Change',
    'Quality Issue': 'Quality Issue',
  };
  
  // Get all display labels
  static List<String> get displayLabels => reasons.values.toList();
  
  // Get the value to send to ESP32 for a display label
  static String getESP32Value(String label) {
    if (useFullNamesForESP32) {
      // Send the exact display label as-is
      return exactESP32Strings[label] ?? label;
    } else {
      // Use simplified keys
      return reasons.entries
          .firstWhere((entry) => entry.value == label,
              orElse: () => MapEntry('quality', label))
          .key;
    }
  }
}

/// Main model class managing MQTT connection and press state
class PressMonitorModel with ChangeNotifier {
  // Client identification
  final String clientIdentifier = 'FlutterApp-${DateTime.now().millisecondsSinceEpoch}';
  
  // Connection state
  MqttServerClient? _client;
  PressState _pressState = PressState.idle;
  bool _isConnected = false;
  String _connectionMessage = 'Disconnected';
  Timer? _connectionTimer;
  
  // Public getters
  PressState get pressState => _pressState;
  bool get isConnected => _isConnected;
  String get connectionMessage => _connectionMessage;
  String get server => AppConstants.mqttServer;
  
  /// Establishes connection to MQTT broker with timeout handling
  Future<void> connect() async {
    if (_isConnected) {
      debugPrint('[MQTT] Already connected');
      return;
    }
    
    if (_client != null && _client?.connectionStatus?.state == MqttConnectionState.connecting) {
      debugPrint('[MQTT] Connection already in progress');
      return;
    }
    
    _initializeClient();
    _updateConnectionStatus('Connecting...');
    
    // Set connection timeout
    _connectionTimer?.cancel();
    _connectionTimer = Timer(AppConstants.connectionTimeout, () {
      if (!_isConnected) {
        debugPrint('[MQTT] Connection timeout');
        _client?.disconnect();
        _onDisconnected();
      }
    });
    
    try {
      await _client!.connect();
    } catch (e) {
      debugPrint('[MQTT] Connection failed: $e');
      _connectionTimer?.cancel();
      _onDisconnected();
    }
  }
  
  /// Initializes MQTT client with proper configuration
  void _initializeClient() {
    _client = MqttServerClient.withPort(
      AppConstants.mqttServer, 
      clientIdentifier, 
      AppConstants.mqttPort
    );
    
    _client!
      ..logging(on: true)  // ⚠️ DEBUGGING: Set to false for production
      ..keepAlivePeriod = AppConstants.keepAlivePeriod
      ..onConnected = _onConnected
      ..onDisconnected = _onDisconnected
      ..onSubscribed = _onSubscribed
      ..autoReconnect = true;
    
    // Configure will message for ungraceful disconnections
    _client!.connectionMessage = MqttConnectMessage()
        .withClientIdentifier(clientIdentifier)
        .withWillTopic('debug/flutter_app')
        .withWillMessage('Flutter client disconnected unexpectedly')
        .withWillQos(MqttQos.atLeastOnce)
        .withWillRetain()
        .startClean();
  }
  
  /// Terminates MQTT connection gracefully
  void disconnect() {
    _connectionTimer?.cancel();
    _client?.disconnect();
    _onDisconnected();
  }
  
  /// Sends operator-selected reason to ESP32
  void sendReason(String reasonLabel) {
    if (_client == null || !_isConnected) {
      debugPrint('[MQTT] Cannot send reason: Not connected');
      return;
    }
    
    // Get the appropriate value for ESP32 based on configuration
    final reasonValue = StopReasons.getESP32Value(reasonLabel);
    
    final payload = jsonEncode({
      "command": "select_reason",
      "reason": reasonValue,
      "timestamp": DateTime.now().toIso8601String(),
    });
    
    final builder = MqttClientPayloadBuilder();
    builder.addString(payload);
    
    debugPrint('[MQTT] Sending reason "$reasonLabel" as "$reasonValue"');
    debugPrint('[MQTT] Full payload: $payload');
    _client!.publishMessage(
      AppConstants.commandsTopic,
      MqttQos.atLeastOnce,
      builder.payload!,
    );
    
    // Provide haptic feedback
    HapticFeedback.mediumImpact();
  }
  
  // ==================== MQTT Event Handlers ====================
  
  void _onConnected() {
    debugPrint('[MQTT] Connected successfully');
    _connectionTimer?.cancel();
    _isConnected = true;
    _updateConnectionStatus('Connected');
    
    // Subscribe to status updates
    _client!.subscribe(AppConstants.statusTopic, MqttQos.atLeastOnce);
    
    // Set up message listener
    _client!.updates!.listen(_handleIncomingMessage);
  }
  
  void _onDisconnected() {
    debugPrint('[MQTT] Disconnected');
    _connectionTimer?.cancel();
    _isConnected = false;
    _pressState = PressState.idle;
    _updateConnectionStatus('Disconnected');
  }
  
  void _onSubscribed(String topic) {
    debugPrint('[MQTT] Subscribed to topic: $topic');
  }
  
  void _handleIncomingMessage(List<MqttReceivedMessage<MqttMessage>> messages) {
    final MqttPublishMessage publishMessage = messages[0].payload as MqttPublishMessage;
    final String payload = MqttPublishPayload.bytesToStringAsString(
      publishMessage.payload.message
    );
    
    debugPrint('[MQTT] Received: $payload');
    
    try {
      final Map<String, dynamic> data = jsonDecode(payload);
      final String? stateString = data['press1'];
      
      if (stateString != null) {
        _updatePressState(stateString);
      }
    } catch (e) {
      debugPrint('[MQTT] Error parsing message: $e');
    }
  }
  
  void _updatePressState(String stateString) {
    final previousState = _pressState;
    
    switch (stateString.toUpperCase()) {
      case 'RUNNING':
        _pressState = PressState.running;
        break;
      case 'WAITING_FOR_REASON':
        _pressState = PressState.waiting;
        break;
      case 'IDLE':
      default:
        _pressState = PressState.idle;
        break;
    }
    
    if (previousState != _pressState) {
      debugPrint('[State] Changed from $previousState to $_pressState');
      notifyListeners();
    }
  }
  
  void _updateConnectionStatus(String message) {
    _connectionMessage = message;
    notifyListeners();
  }
  
  @override
  void dispose() {
    _connectionTimer?.cancel();
    disconnect();
    super.dispose();
  }
}

// ============================================================================
// APPLICATION ENTRY POINT
// ============================================================================

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  
  // Set preferred orientations if needed
  SystemChrome.setPreferredOrientations([
    DeviceOrientation.portraitUp,
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);
  
  runApp(
    ChangeNotifierProvider(
      create: (context) => PressMonitorModel(),
      child: const PressMonitorApp(),
    ),
  );
}

// ============================================================================
// APPLICATION WIDGET
// ============================================================================

class PressMonitorApp extends StatelessWidget {
  const PressMonitorApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Press Monitor',
      debugShowCheckedModeBanner: false,
      theme: _buildTheme(),
      home: const WelcomeScreen(),
    );
  }
  
  ThemeData _buildTheme() {
    return ThemeData(
      brightness: Brightness.light,
      primaryColor: AppColors.primary,
      scaffoldBackgroundColor: AppColors.background,
      fontFamily: 'Inter',
      
      appBarTheme: const AppBarTheme(
        elevation: 1,
        backgroundColor: AppColors.surface,
        foregroundColor: AppColors.textPrimary,
        centerTitle: true,
        titleTextStyle: TextStyle(
          fontSize: 20,
          fontWeight: FontWeight.w600,
          color: AppColors.textPrimary,
        ),
      ),
      
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: AppColors.primary,
          foregroundColor: Colors.white,
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(AppConstants.borderRadius),
          ),
        ),
      ),
      
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(AppConstants.borderRadius),
          ),
        ),
      ),
    );
  }
}


// ============================================================================
// WELCOME SCREEN
// ============================================================================

class WelcomeScreen extends StatelessWidget {
  const WelcomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      body: GestureDetector(
        onTap: () {
          Navigator.of(context).pushReplacement(
            MaterialPageRoute(
              builder: (context) => const PressMonitorPage(),
            ),
          );
        },
        child: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              // Logo
              Container(
                width: 200,
                height: 200,
                padding: const EdgeInsets.all(24),
                decoration: BoxDecoration(
                  color: AppColors.primary.withOpacity(0.1),
                  shape: BoxShape.circle,
                ),
                child: Image.asset(
                  'assets/empower.png',
                  fit: BoxFit.contain,
                ),
              ),
              const SizedBox(height: 48),
              
              // Press to start text
              const Text(
                'Press to start',
                style: TextStyle(
                  fontSize: 24,
                  fontWeight: FontWeight.w600,
                  color: AppColors.textSecondary,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

// ============================================================================
// MAIN PAGE WIDGET
// ============================================================================

class PressMonitorPage extends StatefulWidget {
  const PressMonitorPage({super.key});

  @override
  State<PressMonitorPage> createState() => _PressMonitorPageState();
}

class _PressMonitorPageState extends State<PressMonitorPage>
    with SingleTickerProviderStateMixin {
  late AnimationController _animationController;

  @override
  void initState() {
    super.initState();
    
    // Initialize animation controller for gauge rotation
    _animationController = AnimationController(
      vsync: this,
      duration: AppConstants.gaugeAnimationDuration,
    )..repeat();

    // Auto-connect on startup
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<PressMonitorModel>().connect();
    });
  }

  @override
  void dispose() {
    _animationController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final model = context.watch<PressMonitorModel>();

    return Scaffold(
      appBar: AppBar(
        title: const Text('Press Monitor'),
        actions: [
          _buildConnectionIndicator(model),
        ],
      ),
      body: SafeArea(
        child: _buildBody(context, model),
      ),
    );
  }
  
  /// Builds the connection status indicator for the app bar
  Widget _buildConnectionIndicator(PressMonitorModel model) {
    return Padding(
      padding: const EdgeInsets.only(right: 16),
      child: Row(
        children: [
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(
              color: model.isConnected ? AppColors.success : AppColors.danger,
              shape: BoxShape.circle,
            ),
          ),
          const SizedBox(width: 8),
          Text(
            model.isConnected ? 'Connected' : 'Offline',
            style: AppTextStyles.caption,
          ),
        ],
      ),
    );
  }
  
  /// Main body content with responsive layout
  Widget _buildBody(BuildContext context, PressMonitorModel model) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final isWideScreen = constraints.maxWidth > 800;
        
        return SingleChildScrollView(
          padding: const EdgeInsets.all(AppConstants.defaultPadding),
          child: Center(
            child: Container(
              constraints: const BoxConstraints(maxWidth: 1200),
              child: Column(
                children: [
                  if (model.pressState == PressState.waiting && isWideScreen)
                    _buildWideWaitingLayout(model)
                  else if (model.pressState == PressState.waiting)
                    _buildNarrowWaitingLayout(model)
                  else
                    _buildDefaultLayout(model),
                  
                ],
              ),
            ),
          ),
        );
      },
    );
  }
  
  /// Layout for wide screens when in waiting state
  Widget _buildWideWaitingLayout(PressMonitorModel model) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Left: Reason buttons
        Expanded(
          flex: 4,
          child: _buildReasonButtons(model),
        ),
        
        const SizedBox(width: AppConstants.defaultPadding * 2),
        
        // Right: Gauge and status
        Expanded(
          flex: 6,
          child: _buildGaugeSection(model),
        ),
      ],
    );
  }
  
  /// Layout for narrow screens when in waiting state
  Widget _buildNarrowWaitingLayout(PressMonitorModel model) {
    return Column(
      children: [
        _buildGaugeSection(model),
        const SizedBox(height: AppConstants.defaultPadding * 2),
        _buildReasonButtons(model),
      ],
    );
  }
  
  /// Default layout for idle and running states
  Widget _buildDefaultLayout(PressMonitorModel model) {
    return _buildGaugeSection(model);
  }
  
  /// Builds the gauge visualization section
  Widget _buildGaugeSection(PressMonitorModel model) {
    return Column(
      children: [
        _buildGauge(model),
        const SizedBox(height: AppConstants.defaultPadding),
        _buildStatusText(model.pressState),
      ],
    );
  }
  
  /// Builds the animated gauge widget
  Widget _buildGauge(PressMonitorModel model) {
    return Container(
      width: AppConstants.gaugeSize,
      height: AppConstants.gaugeSize,
      decoration: BoxDecoration(
        color: AppColors.surface,
        shape: BoxShape.circle,
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.1),
            blurRadius: 10,
            offset: const Offset(0, 4),
          ),
        ],
      ),
      child: AnimatedBuilder(
        animation: _animationController,
        builder: (context, child) {
          return CustomPaint(
            painter: PressGaugePainter(
              state: model.pressState,
              animationValue: model.pressState == PressState.running
                  ? _animationController.value
                  : 0,
            ),
            child: Center(
              child: _buildGaugeCenter(model),
            ),
          );
        },
      ),
    );
  }
  
  /// Builds the center content of the gauge
  Widget _buildGaugeCenter(PressMonitorModel model) {
    String displayText;
    
    switch (model.pressState) {
      case PressState.running:
        displayText = '${(_animationController.value * 360).floor()}°';
        break;
      case PressState.waiting:
        displayText = '!';
        break;
      case PressState.idle:
      default:
        displayText = '0°';
    }
    
    return Text(
      displayText,
      style: AppTextStyles.gaugeValue,
    );
  }
  
  /// Builds the status text widget
  Widget _buildStatusText(PressState state) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      decoration: BoxDecoration(
        color: _getStatusColor(state).withOpacity(0.1),
        borderRadius: BorderRadius.circular(20),
      ),
      child: Text(
        _getStatusText(state),
        style: AppTextStyles.subheading.copyWith(
          color: _getStatusColor(state),
        ),
      ),
    );
  }
  
  /// Builds the reason selection buttons
  Widget _buildReasonButtons(PressMonitorModel model) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          'Select Stop Reason',
          style: AppTextStyles.heading,
          textAlign: TextAlign.center,
        ),
        const SizedBox(height: AppConstants.defaultPadding),
        ...StopReasons.displayLabels.map((reason) => Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: _buildReasonButton(model, reason),
        )).toList(),
      ],
    );
  }
  
  /// Builds individual reason button
  Widget _buildReasonButton(PressMonitorModel model, String reason) {
    final color = _getReasonButtonColor(reason);
    
    return OutlinedButton(
      style: OutlinedButton.styleFrom(
        foregroundColor: color,
        side: BorderSide(color: color, width: 2),
        padding: const EdgeInsets.symmetric(vertical: 20),
        minimumSize: const Size.fromHeight(AppConstants.buttonHeight),
      ),
      onPressed: () {
        model.sendReason(reason);
        _showReasonSentFeedback(context, reason);
      },
      child: Text(
        reason,
        style: const TextStyle(
          fontSize: 18,
          fontWeight: FontWeight.w600,
        ),
      ),
    );
  }
  
  /// Shows feedback when reason is sent
  void _showReasonSentFeedback(BuildContext context, String reason) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text('Sent: $reason'),
        duration: const Duration(seconds: 2),
        backgroundColor: AppColors.success,
      ),
    );
  }
  
  /// Builds the connection control widget
  Widget _buildConnectionControl(PressMonitorModel model) {
    if (model.isConnected) {
      return _buildConnectionStatus(model);
    } else if (model.connectionMessage == 'Connecting...') {
      return _buildConnectingStatus();
    } else {
      return _buildConnectButton(model);
    }
  }
  
  /// Builds connected status display
  Widget _buildConnectionStatus(PressMonitorModel model) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: AppColors.success.withOpacity(0.1),
        borderRadius: BorderRadius.circular(AppConstants.borderRadius),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.check_circle, color: AppColors.success, size: 20),
          const SizedBox(width: 8),
          Text(
            'Connected to ${model.server}',
            style: AppTextStyles.body.copyWith(color: AppColors.success),
          ),
        ],
      ),
    );
  }
  
  /// Builds connecting status display
  Widget _buildConnectingStatus() {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        const SizedBox(
          width: 16,
          height: 16,
          child: CircularProgressIndicator(
            strokeWidth: 2,
            valueColor: AlwaysStoppedAnimation<Color>(AppColors.primary),
          ),
        ),
        const SizedBox(width: 12),
        Text(
          'Connecting...',
          style: AppTextStyles.body.copyWith(color: AppColors.primary),
        ),
      ],
    );
  }
  
  /// Builds the connect button
  Widget _buildConnectButton(PressMonitorModel model) {
    return ElevatedButton.icon(
      icon: const Icon(Icons.power_settings_new),
      label: const Text('Connect to Press'),
      style: ElevatedButton.styleFrom(
        padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 16),
      ),
      onPressed: model.connect,
    );
  }
  
  // ==================== Helper Methods ====================
  
  Color _getStatusColor(PressState state) {
    switch (state) {
      case PressState.running:
        return AppColors.success;
      case PressState.waiting:
        return AppColors.warning;
      case PressState.idle:
      default:
        return AppColors.danger;
    }
  }
  
  String _getStatusText(PressState state) {
    switch (state) {
      case PressState.running:
        return 'RUNNING';
      case PressState.waiting:
        return 'WAITING FOR INPUT';
      case PressState.idle:
      default:
        return 'STOPPED';
    }
  }
  
  Color _getReasonButtonColor(String reason) {
    switch (reason) {
      case 'Maintenance Required':
        return AppColors.warning;
      case 'Material Issue':
        return AppColors.primary;
      case 'Tool Change':
        return const Color(0xFF8B5CF6); // violet
      case 'Quality Issue':
        return AppColors.danger;
      default:
        return AppColors.primary;
    }
  }
}

// ============================================================================
// CUSTOM PAINTERS
// ============================================================================

/// Custom painter for the circular gauge visualization
class PressGaugePainter extends CustomPainter {
  final PressState state;
  final double animationValue; // 0.0 to 1.0
  
  const PressGaugePainter({
    required this.state,
    required this.animationValue,
  });
  
  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2 - AppConstants.gaugeStrokeWidth;
    const strokeWidth = AppConstants.gaugeStrokeWidth;
    const startAngle = -math.pi / 2; // Start at 12 o'clock
    
    // Draw background track
    _drawBackgroundTrack(canvas, center, radius, strokeWidth);
    
    // Draw foreground arc
    _drawForegroundArc(canvas, center, radius, strokeWidth, startAngle);
  }
  
  void _drawBackgroundTrack(Canvas canvas, Offset center, double radius, double strokeWidth) {
    final trackPaint = Paint()
      ..color = AppColors.background
      ..style = PaintingStyle.stroke
      ..strokeWidth = strokeWidth;
    
    canvas.drawCircle(center, radius, trackPaint);
  }
  
  void _drawForegroundArc(Canvas canvas, Offset center, double radius, 
                          double strokeWidth, double startAngle) {
    final rect = Rect.fromCircle(center: center, radius: radius);
    
    final arcPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = strokeWidth
      ..strokeCap = StrokeCap.round;
    
    double sweepAngle;
    
    switch (state) {
      case PressState.running:
        arcPaint.color = AppColors.success;
        sweepAngle = animationValue * 2 * math.pi;
        break;
      case PressState.waiting:
        arcPaint.color = AppColors.warning;
        sweepAngle = 2 * math.pi;
        break;
      case PressState.idle:
      default:
        arcPaint.color = AppColors.danger;
        sweepAngle = 2 * math.pi;
        break;
    }
    
    if (sweepAngle > 0) {
      canvas.drawArc(rect, startAngle, sweepAngle, false, arcPaint);
    }
  }
  
  @override
  bool shouldRepaint(covariant PressGaugePainter oldDelegate) {
    return oldDelegate.state != state ||
           oldDelegate.animationValue != animationValue;
  }
}
