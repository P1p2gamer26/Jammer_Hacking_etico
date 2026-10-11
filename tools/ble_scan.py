import asyncio
from bleak import BleakScanner

async def main():
    devices = await BleakScanner.discover(timeout=8.0, return_adv=True)
    print(f"{'NOMBRE':<24} {'DIRECCION':<20} {'RSSI':>6}  SERVICIOS")
    print("-" * 100)
    for d, adv in devices.items():
        name = adv.local_name or "(sin nombre)"
        svc = ",".join(adv.service_uuids or [])[:28]
        print(f"{name:<24} {d:<20} {adv.rssi:>6}  {svc}")

asyncio.run(main())