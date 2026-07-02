# Native Geant4 Ore-Powell Reference

This directory contains a minimal standalone reference executable that
directly calls Geant4's `G4OrePowellAtRestModel`.

It intentionally does not include or link PsSource model, provider,
generator, or truth classes.

## Purpose

The program provides a native-Geant4 integration reference for comparison
with PsSource output.

This comparison tests whether PsSource faithfully preserves the photon
energies and directions returned by the native Geant4 Ore-Powell model.

It is not treated as an independent validation of the Ore-Powell physics.
Independent physical validation is performed against the analytic
Ore-Powell density.

## Build

```bash
bash build.sh
