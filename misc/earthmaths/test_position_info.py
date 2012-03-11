import math
from . import position_info

class TestPositionInfo(object):
    def equal_sf(self, a, b, sf=3):
        if max(a, b) == 0:
            p = 0
        else:
            p = math.log10(max(a, b))
        p -= sf - 1
        tolerance = 10 ** p
        return abs(a - b) < tolerance

    def test_agrees_with_others(self):
        tests = [((51.51, 100.123, 1000), (51.82, 100.2, 3000)),
                 ((-43.0, -3.1, 300000), (-45.1, -2.2, 0)),
                 ((32.712, -20.5, -200), (31.0, -10.1, 500000)),
                 ((-43.0, -3.1, 300000), (51.82, 100.2, 3000)),
                 ((31.0, -10.1, 500000), (51.51, 100.123, 1000)),
                 ((32.712, -20.5, -200), (32.712, -20.5, 10000))]

        for me, balloon in tests:
            a = position_info(me, balloon)
            b = cusf_balloon_azel(*(me + balloon))
            c = dlfldigi(*(me + balloon))

            assert self.equal_sf(a["bearing"], b[0])
            assert self.equal_sf(abs(a["elevation"]), b[1])
            assert self.equal_sf(a["bearing"], c[0])
            assert self.equal_sf(a["great_circle_distance"], c[1])

    # TODO: could perhaps do with some more tests.

# From CUSpaceflight's azel yagi controller; with a couple of comments added
# https://github.com/cuspaceflight/RadioFox/blob/master/interface/interface2.py
def cusf_balloon_azel(lat,lon,alt,Ball_lat,Ball_lon,Ball_alt):
    Rearth = 6378100

    lat=math.radians(lat)
    lon=math.radians(lon)
    Ball_lat=math.radians(Ball_lat)
    Ball_lon=math.radians(Ball_lon)

    # work out heading. Uses the suggested heading formula at
    # http://en.wikipedia.org/wiki/Great-circle_navigation
    # which appears to be from Vincenty's formulae
    head = math.degrees(
        math.atan2( math.sin(Ball_lon-lon) * math.cos(Ball_lat),
                    math.cos(lat) * math.sin(Ball_lat) -
                        math.sin(lat) * math.cos(Ball_lat) *
                        math.cos(Ball_lon-lon)))

    # finds angle at centre using spherical coordinates to make 3d vectors
    # then dot product
    # (unnecessary magnitude multiplcation then division)
    x1=(Rearth+alt)*math.sin(math.pi/2-lat)*math.cos(lon)
    y1=(Rearth+alt)*math.sin(math.pi/2-lat)*math.sin(lon)
    z1=(Rearth+alt)*math.cos(math.pi/2-lat)
    x2=(Rearth+Ball_alt)*math.sin(math.pi/2-Ball_lat)*math.cos(Ball_lon)
    y2=(Rearth+Ball_alt)*math.sin(math.pi/2-Ball_lat)*math.sin(Ball_lon)
    z2=(Rearth+Ball_alt)*math.cos(math.pi/2-Ball_lat)
    angle = math.acos((x1*x2 +y1*y2 + z1*z2)/((Rearth+alt)*(Rearth+Ball_alt)))

    # cosine rule with triangle (r+h1, r+h2, dist) (angle at centre) finds
    # distance in a straight line
    dist= math.sqrt(math.pow((Rearth+Ball_alt),2) + math.pow((Rearth+alt),2)
            - 2*(Rearth+alt)*(Rearth+Ball_alt)*math.cos(angle))

    # sine rule to find angle at listener inside same triangle as for dist.
    # 90 - a to get elevation (horizon perpendicular to triangle side)
    El = 90.0 - math.degrees(
        math.asin( math.sin(angle) * (Rearth+Ball_alt) / 
                   dist))

    return (head, El)

# haversine formulae as used in on dl-fldigi 10/3/12
def dlfldigi(lat1, lon1, alt1, lat2, lon2, alt2):
    from math import sin, cos, sqrt, atan2, pi

    c = pi / 180
    lat1 *= c
    lon1 *= c
    lat2 *= c
    lon2 *= c

    d_lat = lat2 - lat1
    d_lon = lon2 - lon1

    p = sin(d_lat / 2) ** 2
    q = sin(d_lon / 2) ** 2
    a = p + cos(lat1) * cos(lat2) * q

    t = atan2(sqrt(a), sqrt(1 - a)) * 2
    gcd = t * 6371000 # great circle distance

    y = sin(d_lon) * cos(lat2)
    x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(d_lon)
    bearing = atan2(y, x)
    bearing *= (180 / pi)

    return (bearing, gcd)
