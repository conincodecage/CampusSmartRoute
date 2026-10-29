#!/usr/bin/env python3
"""Smoke-test the Smart Campus local API. Run with the server already started."""
import argparse
import json
import sys
import urllib.error
import urllib.request


def request(base, path):
    url = base.rstrip('/') + path
    try:
        with urllib.request.urlopen(url, timeout=3) as response:
            return response.status, response.read().decode('utf-8')
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode('utf-8')


def check(condition, label):
    if not condition:
        raise AssertionError(label)
    print(f'PASS  {label}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-url', default='http://127.0.0.1:8080', help='Base URL, e.g. http://127.0.0.1:8081')
    args = parser.parse_args()
    try:
        status, body = request(args.base_url, '/')
        check(status == 200 and 'Smart Campus' in body, 'Static home page served')

        status, body = request(args.base_url, '/api/health')
        check(status == 200 and json.loads(body).get('ok') is True, 'Health endpoint')

        status, body = request(args.base_url, '/api/campus')
        campus = json.loads(body)
        check(status == 200 and campus['buildingCount'] == 12 and campus['pathCount'] == 21,
              'Campus data: 12 buildings and 21 paths')

        status, body = request(args.base_url, '/api/route?from=gate&to=library')
        route = json.loads(body)
        route_ids = [building['id'] for building in route.get('path', [])]
        check(status == 200 and route.get('distanceM') == 320 and route_ids == ['gate', 'admin', 'library'],
              'Dijkstra: Main Gate -> Admin Block -> Central Library = 320 m')

        status, body = request(args.base_url, '/api/route?from=library&to=library')
        same = json.loads(body)
        check(status == 200 and same.get('distanceM') == 0 and len(same.get('path', [])) == 1,
              'Same source and destination has 0 m distance')

        status, body = request(args.base_url, '/api/route?from=unknown&to=library')
        invalid = json.loads(body)
        check(status == 400 and invalid.get('ok') is False, 'Invalid building returns HTTP 400')

        status_p, body_p = request(args.base_url, '/api/network?algorithm=prim')
        status_k, body_k = request(args.base_url, '/api/network?algorithm=kruskal')
        prim, kruskal = json.loads(body_p), json.loads(body_k)
        check(status_p == 200 and status_k == 200 and prim['connected'] and kruskal['connected']
              and prim['edgeCount'] == 11 and kruskal['edgeCount'] == 11,
              'Prim and Kruskal return connected MSTs with 11 edges')
        check(prim['totalDistanceM'] == kruskal['totalDistanceM'] == 1665,
              'Prim and Kruskal minimum total weight agrees at 1665 m')

        status, body = request(args.base_url, '/api/search?q=library')
        search = json.loads(body)
        check(status == 200 and search['count'] == 1 and search['results'][0]['id'] == 'library',
              'Linear search finds the Central Library')

        status, body = request(args.base_url, '/api/search?q=no-such-place')
        miss = json.loads(body)
        check(status == 200 and miss['count'] == 0, 'Search with no match returns an empty result')

        status, body = request(args.base_url, '/api/buildings?sort=name')
        directory = json.loads(body)
        names = [building['name'] for building in directory['buildings']]
        check(status == 200 and names == sorted(names), 'Alphabetical directory sorting')
    except (AssertionError, KeyError, ValueError, urllib.error.URLError, TimeoutError) as exc:
        print(f'FAIL  {exc}', file=sys.stderr)
        if isinstance(exc, urllib.error.URLError):
            print('Start the server before running this test script.', file=sys.stderr)
        return 1
    print('\nAll API smoke tests passed.')
    print('Note: these tests do not replace manual testing of browser interactions.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
