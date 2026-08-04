import java.net.*;

/**
 * Reproduces R4ListenerService.openSocketAndReceive()'s receive loop verbatim:
 * one DatagramPacket allocated outside the loop, no setLength() before receive.
 * Then the same loop WITH the one-line fix, for comparison.
 */
public class TruncationRepro {
    static final int PORT = 47399;
    static final int BUFFER_SIZE = 4096;

    // Sizes taken from the branch's own mock sender: state heartbeat then warnings.
    static final int[] SIZES = {367, 144, 367, 367, 373};

    public static void main(String[] args) throws Exception {
        System.out.println("sent sizes: 367 (warning), 144 (state), 367, 367, 373 (unknown-type)\n");
        run(false);
        System.out.println();
        run(true);
    }

    static void run(boolean withFix) throws Exception {
        System.out.println(withFix ? "=== WITH setLength() fix ===" : "=== AS ON THE BRANCH (no setLength) ===");
        try (DatagramSocket socket = new DatagramSocket(PORT)) {
            Thread sender = new Thread(() -> {
                try (DatagramSocket out = new DatagramSocket()) {
                    Thread.sleep(200);
                    for (int size : SIZES) {
                        byte[] payload = new byte[size];
                        java.util.Arrays.fill(payload, (byte) 'x');
                        out.send(new DatagramPacket(payload, payload.length,
                                InetAddress.getByName("127.0.0.1"), PORT));
                        Thread.sleep(100);
                    }
                } catch (Exception e) { e.printStackTrace(); }
            });
            sender.start();

            byte[] buffer = new byte[BUFFER_SIZE];
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            socket.setSoTimeout(3000);
            for (int i = 0; i < SIZES.length; i++) {
                if (withFix) packet.setLength(buffer.length);   // the missing line
                try {
                    socket.receive(packet);
                } catch (SocketTimeoutException e) {
                    System.out.println("  timed out waiting for datagram " + (i + 1));
                    break;
                }
                int got = packet.getLength();
                int sent = SIZES[i];
                System.out.printf("  sent %3d B -> read %3d B  %s%n",
                        sent, got, got == sent ? "ok" : "*** TRUNCATED, " + (sent - got) + " B lost ***");
            }
            sender.join();
        }
    }
}
