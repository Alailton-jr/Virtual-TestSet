import { useState } from 'react'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select'
import { Badge } from '@/components/ui/badge'
import { Play, Square } from 'lucide-react'
import { useStreamStore } from '@/stores/useStreamStore'

export default function RampingTestPage() {
  const { streams } = useStreamStore()
  const [selectedStreamId, setSelectedStreamId] = useState<string>('')
  const [variable, setVariable] = useState<string>('voltage')
  const [startValue, setStartValue] = useState('0')
  const [endValue, setEndValue] = useState('150')
  const [stepValue, setStepValue] = useState('5')
  const [durationSec, setDurationSec] = useState('0.5')
  const [isRunning, setIsRunning] = useState(false)
  const [results, setResults] = useState({ pickup: 0, dropout: 0, reset: 0 })

  const handleStartTest = () => {
    setIsRunning(true)
    setTimeout(() => {
      setResults({ pickup: 110.5, dropout: 95.2, reset: 88.3 })
      setIsRunning(false)
    }, 3000)
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">Ramping Test</h1>
        <p className="text-muted-foreground">
          Automated ramping test for pickup/dropout determination
        </p>
      </div>

      <div className="grid gap-6 lg:grid-cols-3">
        <Card className="lg:col-span-2">
          <CardHeader>
            <CardTitle>Ramp Configuration</CardTitle>
            <CardDescription>
              Configure variable, range, step size, and stop conditions
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="grid gap-4 sm:grid-cols-2">
              <div className="space-y-2">
                <Label htmlFor="stream">Target Stream</Label>
                <Select value={selectedStreamId} onValueChange={setSelectedStreamId}>
                  <SelectTrigger id="stream"><SelectValue placeholder="Select stream" /></SelectTrigger>
                  <SelectContent>{streams.map((s) => <SelectItem key={s.id} value={s.id}>{s.name}</SelectItem>)}</SelectContent>
                </Select>
              </div>
              <div className="space-y-2">
                <Label htmlFor="variable">Variable</Label>
                <Select value={variable} onValueChange={setVariable}>
                  <SelectTrigger id="variable"><SelectValue /></SelectTrigger>
                  <SelectContent>
                    <SelectItem value="voltage">Voltage Magnitude</SelectItem>
                    <SelectItem value="current">Current Magnitude</SelectItem>
                    <SelectItem value="frequency">Frequency</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>
            <div className="grid gap-4 sm:grid-cols-3">
              <div className="space-y-2">
                <Label htmlFor="start">Start Value</Label>
                <Input id="start" type="number" value={startValue} onChange={(e) => setStartValue(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="end">End Value</Label>
                <Input id="end" type="number" value={endValue} onChange={(e) => setEndValue(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="step">Step Size</Label>
                <Input id="step" type="number" value={stepValue} onChange={(e) => setStepValue(e.target.value)} />
              </div>
            </div>
            <div className="grid gap-4 sm:grid-cols-2">
              <div className="space-y-2">
                <Label htmlFor="duration">Duration per Step (sec)</Label>
                <Input id="duration" type="number" step="0.1" value={durationSec} onChange={(e) => setDurationSec(e.target.value)} />
              </div>
            </div>
            <div className="flex gap-2">
              <Button onClick={handleStartTest} disabled={!selectedStreamId || isRunning} className="flex-1">
                {isRunning ? <><Square className="mr-2 h-4 w-4" />Stop Test</> : <><Play className="mr-2 h-4 w-4" />Start Ramp</>}
              </Button>
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader><CardTitle>Test KPIs</CardTitle><CardDescription>Key performance indicators</CardDescription></CardHeader>
          <CardContent className="space-y-3">
            <div className="flex items-center justify-between p-2 border rounded">
              <span className="text-sm font-medium">Pickup</span>
              <Badge variant={results.pickup > 0 ? 'default' : 'outline'}>{results.pickup > 0 ? `${results.pickup} V` : 'N/A'}</Badge>
            </div>
            <div className="flex items-center justify-between p-2 border rounded">
              <span className="text-sm font-medium">Dropout</span>
              <Badge variant={results.dropout > 0 ? 'default' : 'outline'}>{results.dropout > 0 ? `${results.dropout} V` : 'N/A'}</Badge>
            </div>
            <div className="flex items-center justify-between p-2 border rounded">
              <span className="text-sm font-medium">Reset</span>
              <Badge variant={results.reset > 0 ? 'default' : 'outline'}>{results.reset > 0 ? `${results.reset} V` : 'N/A'}</Badge>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
