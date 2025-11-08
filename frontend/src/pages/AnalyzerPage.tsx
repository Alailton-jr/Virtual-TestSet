import { useState } from 'react'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select'
import { Badge } from '@/components/ui/badge'
import { Play, Square } from 'lucide-react'
import { useStreamStore } from '@/stores/useStreamStore'

interface Phasor {
  channel: string
  magnitude: number
  angle: number
}

export default function AnalyzerPage() {
  const { streams } = useStreamStore()
  const [selectedStreamId, setSelectedStreamId] = useState<string>('')
  const [isCapturing, setIsCapturing] = useState(false)
  const [phasors] = useState<Phasor[]>([
    { channel: 'V-A', magnitude: 115.47, angle: 0.0 },
    { channel: 'V-B', magnitude: 115.47, angle: -120.0 },
    { channel: 'V-C', magnitude: 115.47, angle: 120.0 },
    { channel: 'I-A', magnitude: 5.77, angle: -30.0 },
  ])

  const handleCapture = () => {
    setIsCapturing(!isCapturing)
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">Network Analyzer</h1>
        <p className="text-muted-foreground">
          Real-time analysis of SV streams with oscilloscope and phasor visualization
        </p>
      </div>

      <Card>
        <CardHeader>
          <div className="flex items-center justify-between">
            <div><CardTitle>Capture Configuration</CardTitle></div>
            <div className="flex gap-2">
              <Select value={selectedStreamId} onValueChange={setSelectedStreamId}>
                <SelectTrigger className="w-48"><SelectValue placeholder="Select stream" /></SelectTrigger>
                <SelectContent>{streams.map((s) => <SelectItem key={s.id} value={s.id}>{s.name}</SelectItem>)}</SelectContent>
              </Select>
              <Button onClick={handleCapture} disabled={!selectedStreamId}>
                {isCapturing ? <><Square className="mr-2 h-4 w-4" />Stop</> : <><Play className="mr-2 h-4 w-4" />Capture</>}
              </Button>
            </div>
          </div>
        </CardHeader>
      </Card>

      <div className="grid gap-6">
        <Card>
          <CardHeader>
            <CardTitle>Waveform Oscilloscope</CardTitle>
            <CardDescription>
              Time-domain view of voltage and current waveforms
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="h-64 flex items-center justify-center border rounded-lg bg-muted/20">
              {isCapturing ? (
                <div className="animate-pulse h-48 w-full bg-gradient-to-r from-blue-500/20 via-green-500/20 to-red-500/20 rounded" />
              ) : (
                <p className="text-sm text-muted-foreground">Click Capture to start acquisition</p>
              )}
            </div>
          </CardContent>
        </Card>

        <div className="grid gap-6 md:grid-cols-2">
          <Card>
            <CardHeader>
              <CardTitle>Phasor Diagram</CardTitle>
              <CardDescription>Vector representation of phasors</CardDescription>
            </CardHeader>
            <CardContent>
              {isCapturing ? (
                <div className="space-y-3">
                  {phasors.map((p, idx) => (
                    <div key={idx} className="flex items-center justify-between p-2 border rounded">
                      <span className="font-medium text-sm">{p.channel}</span>
                      <div className="flex gap-4 text-xs">
                        <span className="text-muted-foreground">Mag: <span className="font-mono text-foreground">{p.magnitude.toFixed(2)}</span> V</span>
                        <span className="text-muted-foreground">∠ <span className="font-mono text-foreground">{p.angle.toFixed(1)}°</span></span>
                      </div>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="h-48 flex items-center justify-center border rounded-lg bg-muted/20">
                  <p className="text-sm text-muted-foreground">No phasor data</p>
                </div>
              )}
            </CardContent>
          </Card>

          <Card>
            <CardHeader>
              <CardTitle>Harmonics Spectrum</CardTitle>
              <CardDescription>Frequency domain analysis</CardDescription>
            </CardHeader>
            <CardContent>
              {isCapturing ? (
                <div className="space-y-3">
                  <div className="flex items-center justify-between">
                    <span className="text-sm font-medium">Total Harmonic Distortion</span>
                    <Badge variant="outline">2.3%</Badge>
                  </div>
                  <div className="space-y-2">
                    {[{ h: 1, pct: 100 }, { h: 3, pct: 5 }, { h: 5, pct: 3 }, { h: 7, pct: 1 }].map((h) => (
                      <div key={h.h} className="flex items-center gap-2">
                        <span className="text-xs w-8">H{h.h}</span>
                        <div className="flex-1 h-6 bg-muted rounded-full overflow-hidden">
                          <div className="h-full bg-blue-500 transition-all" style={{ width: `${h.pct}%` }} />
                        </div>
                        <span className="text-xs w-12 text-right text-muted-foreground">{h.pct}%</span>
                      </div>
                    ))}
                  </div>
                </div>
              ) : (
                <div className="h-48 flex items-center justify-center border rounded-lg bg-muted/20">
                  <p className="text-sm text-muted-foreground">No harmonic data</p>
                </div>
              )}
            </CardContent>
          </Card>
        </div>
      </div>
    </div>
  )
}

