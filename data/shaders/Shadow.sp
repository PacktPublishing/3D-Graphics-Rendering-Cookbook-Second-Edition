//

float PCF3x3(vec3 uvw, uint textureid, uint samplerid) {
  float size = 1.0 / textureBindlessSize2D(textureid).x; // assume square texture
  float shadow = 0.0;
  for (int v=-1; v<=+1; v++)
    for (int u=-1; u<=+1; u++)
      shadow += textureBindless2DShadow(textureid, samplerid, uvw + size * vec3(u, v, 0));
  return shadow / 9;
}

float shadow(vec4 s, uint textureid, uint samplerid) {
  s = s / s.w;
  if (s.z > -1.0 && s.z < 1.0) {
    float shadowSample = PCF3x3(vec3(s.x, 1.0 - s.y, s.z), textureid, samplerid);
    return mix(0.3, 1.0, shadowSample);
  }
  return 1.0;
}
