from flask import Flask, request, jsonify
import mysql.connector
from flask import render_template
from flask_cors import CORS


app = Flask(__name__)
CORS(app)
db_config = {
    'host': 'localhost',
    'user': 'root',
    'password': 'кщще',  # заміни на свій пароль
    'database': 'lora_db'
}

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/api/lora', methods=['POST'])
def receive_lora_data():
    data = request.get_json()

    # Отримуємо всі можливі поля, навіть якщо вони None
    mac = data.get('mac')
    rssi = data.get('rssi')
    dist = data.get('dist')
    temp = data.get('temp')
    press = data.get('press')
    hum = data.get('hum')
    device_id = data.get('device_id')

    try:
        conn = mysql.connector.connect(**db_config)
        cursor = conn.cursor()

        # Вставляємо завжди всі поля, навіть якщо деякі з них None
        cursor.execute("""
            INSERT INTO messages (mac, rssi, dist, temp, press, hum, device_id)
            VALUES (%s, %s, %s, %s, %s, %s, %s)
        """, (mac, rssi, dist, temp, press, hum, device_id))

        conn.commit()
        cursor.close()
        conn.close()
        return jsonify({'message': 'Data saved'}), 201
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/lora', methods=['GET'])
def get_lora_data():
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor(dictionary=True)
    cursor.execute("SELECT * FROM messages ORDER BY timestamp DESC LIMIT 100")
    rows = cursor.fetchall()
    cursor.close()
    conn.close()
    return jsonify(rows)


@app.route('/api/mac-stats', methods=['GET'])
def get_mac_stats():
    conn = mysql.connector.connect(**db_config)
    cursor = conn.cursor()

    # Підраховуємо кількість записів для кожного MAC
    cursor.execute("""
        SELECT mac, COUNT(*) as count
        FROM messages
        WHERE mac IS NOT NULL
        GROUP BY mac
        ORDER BY count DESC
    """)
    results = cursor.fetchall()

    # Перетворюємо в список словників
    mac_data = [{'mac': mac, 'count': count} for mac, count in results]

    cursor.close()
    conn.close()
    return jsonify(mac_data)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
