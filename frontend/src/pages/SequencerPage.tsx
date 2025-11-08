import { useState, useEffect } from 'react'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select'
import { Alert, AlertDescription } from '@/components/ui/alert'
import { Play, Square, Plus, Trash2, AlertCircle } from 'lucide-react'
import { useStreamStore } from '@/stores/useStreamStore'

interface SequenceState {
  name: string
  durationSec: number
  transition: { type: 'time' | 'gooseTrip' }
}

export default function SequencerPage() {
  const { streams, fetchStreams } = useStreamStore()
  const [activeStreams, setActiveStreams] = useState<string[]>([])
  const [states, setStates] = useState<SequenceState[]>([{ name: 'Pre-Fault', durationSec: 2.0, transition: { type: 'time' } }])
  const [isRunning, setIsRunning] = useState(false)
  const [error, setError] = useState<string>('')

  useEffect(() => { fetchStreams() }, [fetchStreams])
  useEffect(() => { if (error) { const timer = setTimeout(() => setError(''), 5000); return () => clearTimeout(timer) } }, [error])

  const addState = () => { setStates([...states, { name: `State ${states.length + 1}`, durationSec: 1.0, transition: { type: 'time' } }]) }
  const removeState = (index: number) => { if (states.length > 1) setStates(states.filter((_, i) => i !== index)) }
  const updateState = (index: number, updates: Partial<SequenceState>) => { setStates(states.map((state, i) => (i === index ? { ...state, ...updates } : state))) }

  const handleStart = async () => {
    if (activeStreams.length === 0) { setError('Select at least one stream'); return }
    setIsRunning(true); setError('')
    try {
      // In production, would use proper sequence API with sequence ID
      // For now, just simulate starting
      setTimeout(() => setIsRunning(false), states.reduce((sum, s) => sum + s.durationSec, 0) * 1000)
    } catch (err) { setError(err instanceof Error ? err.message : 'Failed to start'); setIsRunning(false) }
  }

  const handleStop = async () => {
    try { setIsRunning(false) }
    catch (err) { setError(err instanceof Error ? err.message : 'Failed to stop') }
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">Sequencer</h1>
        <p className="text-muted-foreground">Create and execute automated test sequences</p>
      </div>

      {error && <Alert variant="destructive"><AlertCircle className="h-4 w-4" /><AlertDescription>{error}</AlertDescription></Alert>}

      <Card>
        <CardHeader><CardTitle>Active Streams</CardTitle><CardDescription>Select streams for sequence</CardDescription></CardHeader>
        <CardContent>
          <div className="flex flex-wrap gap-2">
            {streams.map(stream => (
              <label key={stream.id} className="flex items-center gap-2 px-3 py-2 border rounded-lg cursor-pointer hover:bg-muted">
                <input type="checkbox" checked={activeStreams.includes(stream.id)} onChange={(e) => { e.target.checked ? setActiveStreams([...activeStreams, stream.id]) : setActiveStreams(activeStreams.filter(id => id !== stream.id)) }} disabled={isRunning} className="rounded" />
                <span className="text-sm font-medium">{stream.name}</span>
              </label>
            ))}
          </div>
        </CardContent>
      </Card>

      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <h2 className="text-xl font-semibold">Sequence States</h2>
          <Button onClick={addState} disabled={isRunning} size="sm"><Plus className="mr-2 h-4 w-4" />Add State</Button>
        </div>

        {states.map((state, index) => (
          <Card key={index}>
            <CardHeader>
              <div className="flex items-center justify-between">
                <CardTitle className="text-base">State {index + 1}: {state.name}</CardTitle>
                <Button variant="ghost" size="sm" onClick={() => removeState(index)} disabled={isRunning || states.length === 1}><Trash2 className="h-4 w-4" /></Button>
              </div>
            </CardHeader>
            <CardContent>
              <div className="grid grid-cols-3 gap-4">
                <div className="space-y-2">
                  <Label>State Name</Label>
                  <Input value={state.name} onChange={(e) => updateState(index, { name: e.target.value })} disabled={isRunning} />
                </div>
                <div className="space-y-2">
                  <Label>Duration (seconds)</Label>
                  <Input type="number" value={state.durationSec} onChange={(e) => updateState(index, { durationSec: parseFloat(e.target.value) || 0 })} disabled={isRunning} step={0.1} />
                </div>
                <div className="space-y-2">
                  <Label>Transition</Label>
                  <Select value={state.transition.type} onValueChange={(value: 'time' | 'gooseTrip') => updateState(index, { transition: { type: value } })} disabled={isRunning}>
                    <SelectTrigger><SelectValue /></SelectTrigger>
                    <SelectContent><SelectItem value="time">Time-based</SelectItem><SelectItem value="gooseTrip">GOOSE Trip</SelectItem></SelectContent>
                  </Select>
                </div>
              </div>
            </CardContent>
          </Card>
        ))}
      </div>

      <Card>
        <CardHeader><CardTitle>Sequence Control</CardTitle></CardHeader>
        <CardContent className="space-y-4">
          <div className="flex gap-2">
            {!isRunning ? <Button onClick={handleStart} disabled={activeStreams.length === 0} className="flex-1"><Play className="mr-2 h-4 w-4" />Start</Button> : <Button onClick={handleStop} variant="destructive" className="flex-1"><Square className="mr-2 h-4 w-4" />Stop</Button>}
          </div>
          <div className="text-sm text-muted-foreground space-y-1">
            <p>• {activeStreams.length} stream(s), {states.length} state(s)</p>
            <p>• Duration: {states.reduce((sum, s) => sum + s.durationSec, 0).toFixed(1)}s</p>
          </div>
        </CardContent>
      </Card>
    </div>
  )
}
