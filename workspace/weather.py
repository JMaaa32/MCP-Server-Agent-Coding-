import urllib.request
import json

url = "https://wttr.in/Beijing?format=j1"
req = urllib.request.Request(url, headers={"User-Agent": "curl/7.68.0"})
with urllib.request.urlopen(req, timeout=8) as resp:
    data = json.loads(resp.read().decode())

current = data["current_condition"][0]
weather_desc = current["weatherDesc"][0]["value"]
temp_c = current["temp_C"]
feels_like = current["FeelsLikeC"]
humidity = current["humidity"]
wind_speed = current["windspeedKmph"]
wind_dir = current["winddir16Point"]
visibility = current["visibility"]

today = data["weather"][0]
max_temp = today["maxtempC"]
min_temp = today["mintempC"]

print("=== 北京实时天气 ===")
print(f"天气状况：{weather_desc}")
print(f"当前气温：{temp_c}°C（体感 {feels_like}°C）")
print(f"今日最高：{max_temp}°C  最低：{min_temp}°C")
print(f"湿　　度：{humidity}%")
print(f"风　　速：{wind_speed} km/h  风向：{wind_dir}")
print(f"能见度：{visibility} km")

print("\n=== 未来3天预报 ===")
for day in data["weather"]:
    date = day["date"]
    desc = day["hourly"][4]["weatherDesc"][0]["value"]
    hi = day["maxtempC"]
    lo = day["mintempC"]
    print(f"{date}  {desc}  {lo}°C ~ {hi}°C")
